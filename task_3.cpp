#include <boost/asio.hpp>
#include <iostream>
#include <sstream>

using boost::asio::ip::tcp;

class ReminderSession : public std::enable_shared_from_this<ReminderSession> {
public:
    ReminderSession(tcp::socket socket, boost::asio::io_context& io)
        : socket_(std::move(socket)), io_(io) {}
    
    void start() {
        do_read();
    }
    
private:
    void do_read() {
        auto self(shared_from_this());
        boost::asio::async_read_until(socket_, buffer_, '\n',
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string line;
                    std::getline(is, line);
                    
                    if (line.substr(0, 7) == "remind ") {
                        std::istringstream iss(line.substr(7));
                        int seconds;
                        std::string text;
                        
                        if (iss >> seconds && std::getline(iss, text)) {
                            // Удаляем лишние пробелы в начале текста
                            text.erase(0, text.find_first_not_of(" \t"));
                            
                            // Отправляем подтверждение
                            std::string response = "Напоминание будет через " + 
                                                  std::to_string(seconds) + 
                                                  " секунд\n";
                            boost::asio::async_write(
                                socket_, boost::asio::buffer(response),
                                [this, self, seconds, text](boost::system::error_code ec, std::size_t) {
                                    if (!ec) {
                                        // Устанавливаем таймер
                                        auto timer = std::make_shared<boost::asio::steady_timer>(io_);
                                        timer->expires_after(std::chrono::seconds(seconds));
                                        timer->async_wait(
                                            [this, self, text, timer](const boost::system::error_code& ec) {
                                                if (!ec) {
                                                    boost::asio::async_write(
                                                        socket_, boost::asio::buffer(text + "\n"),
                                                        [](boost::system::error_code, std::size_t) {}
                                                    );
                                                }
                                            }
                                        );
                                    }
                                }
                            );
                        } else {
                            boost::asio::async_write(
                                socket_, boost::asio::buffer("Ошибка: неверный формат напоминания\n"),
                                [](boost::system::error_code, std::size_t) {}
                            );
                        }
                    } else {
                        boost::asio::async_write(
                            socket_, boost::asio::buffer("Ошибка: неизвестная команда\n"),
                            [](boost::system::error_code, std::size_t) {}
                        );
                    }
                    
                    do_read();
                }
            }
        );
    }
    
    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    boost::asio::io_context& io_;
};

class ReminderServer {
public:
    ReminderServer(boost::asio::io_context& io, short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), io_(io) {
        do_accept();
    }
    
private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<ReminderSession>(std::move(socket), io_)->start();
                }
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;
    boost::asio::io_context& io_;
};

int main() {
    try {
        boost::asio::io_context io;
        ReminderServer server(io, 12345);
        std::cout << "Сервер напоминаний запущен на порту 12345" << std::endl;
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
    }
    
    return 0;
}
