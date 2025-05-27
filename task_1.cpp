#include <boost/asio.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>

using boost::asio::ip::tcp;

std::string calculate(const std::string& expr) {
    std::istringstream iss(expr);
    double a, b;
    char op;
    
    if (!(iss >> a >> op >> b)) {
        return "Ошибка: неверный формат";
    }
    
    switch (op) {
        case '+': return std::to_string(a + b);
        case '-': return std::to_string(a - b);
        case '*': return std::to_string(a * b);
        case '/': 
            if (b == 0) return "Ошибка: деление на ноль";
            {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << (a / b);
                return oss.str();
            }
        default: return "Ошибка: неизвестная операция";
    }
}

void handle_client(tcp::socket& socket) {
    try {
        boost::asio::streambuf buffer;
        
        while (true) {
            // Чтение данных от клиента
            boost::asio::read_until(socket, buffer, '\n');
            std::istream is(&buffer);
            std::string expr;
            std::getline(is, expr);
            
            if (expr.empty()) break;
            
            // Вычисление результата
            std::string result = calculate(expr) + "\n";
            
            // Отправка результата
            boost::asio::write(socket, boost::asio::buffer(result));
        }
    } catch (std::exception& e) {
        std::cerr << "Ошибка в клиенте: " << e.what() << std::endl;
    }
}

int main() {
    try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 12345));
        
        std::cout << "Сервер калькулятора запущен на порту 12345" << std::endl;
        
        while (true) {
            tcp::socket socket(io);
            acceptor.accept(socket);
            std::cout << "Новое подключение: " 
                      << socket.remote_endpoint() << std::endl;
            
            handle_client(socket);
        }
    } catch (std::exception& e) {
        std::cerr << "Ошибка сервера: " << e.what() << std::endl;
    }
    
    return 0;
}
