#include <boost/asio.hpp>
#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <cmath>

using boost::asio::ip::tcp;

class ThreadSafeLog {
public:
    void add(const std::string& entry) {
        boost::asio::post(strand_, [this, entry]() {
            log_.push_back(entry);
            std::cout << entry << std::endl;
        });
    }
    
    void set_strand(boost::asio::strand<boost::asio::io_context::executor_type>& strand) {
        strand_ = strand;
    }
    
private:
    std::vector<std::string> log_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_{log_.front().get_executor()};
};

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, 
            boost::asio::strand<boost::asio::io_context::executor_type>& strand,
            ThreadSafeLog& log)
        : socket_(std::move(socket)), strand_(strand), log_(log),
          timer_(socket_.get_executor()) {}
    
    void start() {
        log_.add("Новое подключение: " + 
                 socket_.remote_endpoint().address().to_string());
        do_read();
    }
    
private:
    void do_read() {
        auto self(shared_from_this());
        
        // Установка таймаута
        timer_.expires_after(std::chrono::seconds(30));
        timer_.async_wait([this, self](boost::system::error_code ec) {
            if (!ec) {
                socket_.close();
                log_.add("Таймаут подключения");
            }
        });
        
        boost::asio::async_read_until(socket_, buffer_, '\n',
            boost::asio::bind_executor(strand_,
                [this, self](boost::system::error_code ec, std::size_t length) {
                    timer_.cancel();
                    
                    if (!ec) {
                        std::istream is(&buffer_);
                        std::string line;
                        std::getline(is, line);
                        
                        try {
                            int n = std::stoi(line);
                            
                            // Вычисление в фоновом потоке
                            boost::asio::post(
                                [this, self, n]() {
                                    // Долгая операция (например, проверка простоты числа)
                                    bool is_prime = true;
                                    if (n < 2) {
                                        is_prime = false;
                                    } else {
                                        for (int i = 2; i <= std::sqrt(n); ++i) {
                                            if (n % i == 0) {
                                                is_prime = false;
                                                break;
                                            }
                                        }
                                    }
                                    
                                    std::string result = std::to_string(n) + 
                                                        (is_prime ? " - простое" : " - составное");
                                    
                                    // Возвращаемся в strand для записи
                                    boost::asio::post(
                                        strand_,
                                        [this, self, result]() {
                                            log_.add("Результат: " + result);
                                            do_write(result + "\n");
                                        }
                                    );
                                }
                            );
                        } catch (...) {
                            log_.add("Ошибка: неверный формат числа");
                            do_write("Ошибка: введите целое число\n");
                        }
                        
                        do_read();
                    } else {
                        log_.add("Отключение клиента: " + 
                                socket_.remote_endpoint().address().to_string());
                    }
                }
            )
        );
    }
    
    void do_write(const std::string& message) {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(message),
            boost::asio::bind_executor(strand_,
                [this, self](boost::system::error_code ec, std::size_t) {
                    if (ec) {
                        log_.add("Ошибка отправки: " + ec.message());
                    }
                }
            )
        );
    }
    
    tcp::socket socket_;
    boost::asio::strand<boost::asio::io_context::executor_type>& strand_;
    ThreadSafeLog& log_;
    boost::asio::streambuf buffer_;
    boost::asio::steady_timer timer_;
};

class Server {
public:
    Server(boost::asio::io_context& io, short port, int thread_count)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), 
          strand_(boost::asio::make_strand(io)),
          thread_count_(thread_count) {
        log_.set_strand(strand_);
        do_accept();
    }
    
    void run() {
        std::vector<std::thread> threads;
        for (int i = 0; i < thread_count_; ++i) {
            threads.emplace_back([this]() { io_.run(); });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }
    
private:
    void do_accept() {
        acceptor_.async_accept(
            boost::asio::make_strand(io_),
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), strand_, log_)->start();
                }
                do_accept();
            }
        );
    }
    
    boost::asio::io_context io_;
    tcp::acceptor acceptor_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    ThreadSafeLog log_;
    int thread_count_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Использование: server <port> <threads>\n";
            return 1;
        }
        
        short port = std::atoi(argv[1]);
        int threads = std::atoi(argv[2]);
        
        Server server(port, threads);
        std::cout << "Сервер запущен на порту " << port 
                  << " с " << threads << " потоками" << std::endl;
        server.run();
    } catch (std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
    }
    
    return 0;
}
