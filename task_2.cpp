#include <boost/asio.hpp>
#include <iostream>
#include <sstream>
#include <numeric>
#include <vector>

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}
    
    void start() {
        do_read();
    }
    
private:
    void do_read() {
        auto self(shared_from_this());
        boost::asio::async_read_until(socket_, buffer_, '\n',
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string data;
                    std::getline(is, data);
                    
                    // Асинхронная обработка в другом потоке
                    boost::asio::post(
                        [this, self, data]() {
                            std::vector<double> numbers;
                            std::istringstream iss(data);
                            double num;
                            
                            while (iss >> num) {
                                numbers.push_back(num);
                            }
                            
                            if (!numbers.empty()) {
                                double sum = std::accumulate(numbers.begin(), numbers.end(), 0.0);
                                double avg = sum / numbers.size();
                                
                                std::ostringstream oss;
                                oss << "Среднее: " << std::fixed << std::setprecision(2) << avg << "\n";
                                response_ = oss.str();
                            } else {
                                response_ = "Ошибка: нет чисел для вычисления\n";
                            }
                            
                            // Возвращаемся в контекст ввода-вывода для отправки
                            boost::asio::post(
                                socket_.get_executor(),
                                [this, self]() { do_write(); }
                            );
                        }
                    );
                }
            }
        );
    }
    
    void do_write() {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(response_),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    do_read();
                }
            }
        );
    }
    
    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    std::string response_;
};

class Server {
public:
    Server(boost::asio::io_context& io, short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }
    
private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;
};

int main() {
    try {
        boost::asio::io_context io;
        Server server(io, 12345);
        std::cout << "Сервер среднего запущен на порту 12345" << std::endl;
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
    }
    
    return 0;
}
