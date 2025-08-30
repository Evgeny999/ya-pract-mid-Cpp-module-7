#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <print>
#include <string_view>

using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::dynamic_buffer;
using boost::asio::io_service;
using boost::asio::transfer_at_least;
using boost::asio::use_awaitable;
using namespace boost::asio::experimental::awaitable_operators;
using boost::asio::ip::tcp;
using boost::system::error_code;

constexpr std::string_view delimiter = "\r\n\r\n";
// Подключаемся к серверу
awaitable<tcp::socket> connectToTarget(io_service &io_service, const std::string &host, const std::string &port) {
    unsigned short port_num = static_cast<unsigned short>(std::stoi(port));

    boost::asio::ip::address address = boost::asio::ip::address::from_string(host);
    tcp::endpoint endpoint(address, port_num);

    tcp::socket target_socket(io_service);
    co_await target_socket.async_connect(endpoint, use_awaitable);

    co_return target_socket;
}
// Передача данных между клиентом и сервером
awaitable<void> forwardData(tcp::socket &from, tcp::socket &to, const std::string &direction) {
    try {
        std::string buffer(4096, '\0');

        while (true) {
            error_code ec;
            std::size_t bytes_read = co_await from.async_read_some(boost::asio::buffer(buffer),
                                                                   boost::asio::redirect_error(use_awaitable, ec));
            auto contentLength = findContentLength(buffer);
            if (contentLength.has_value()) {
                std::println("Content-Length detected: {} bytes", contentLength.value());
            }

            if (ec == boost::asio::error::eof) {
                // Graceful connection closure
                std::println("{} connection closed gracefully", direction);
                break;
            } else if (ec) {
                // Other error
                throw boost::system::system_error(ec);
            }

            co_await async_write(to, boost::asio::buffer(buffer, bytes_read), use_awaitable);
            std::println("{} forwarded {} bytes", direction, bytes_read);
        }
    } catch (const std::exception &e) {
        std::println("Forwarding error ({}): {}", direction, e.what());
        throw;  // Re-throw to cancel the other direction
    }
}

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {
    std::string client_id = "unknown";
    auto log = [&client_id](std::string_view msg) { std::println("[{}] {}.", client_id, msg); };
    try {
        std::string buf;
        client_id = std::format("{}:{}", client_socket.remote_endpoint().address().to_string(),
                                client_socket.remote_endpoint().port());
        log("Client connected");
        try {
            /*std::size_t n = */ co_await async_read_until(client_socket, dynamic_buffer(buf), '\n', use_awaitable);
            // std::println("buf = {}", buf);
            // Нужно взять ip сервера и tcp порт из запроса клиента
            auto [host, port] = findHostPort(buf);
            log(std::format("Connecting to target: {}:{}", host, port));

            std::println("host = '{}'", host);
            std::println("port = '{}'", port);

            // Connect to target server
            tcp::socket target_socket = co_await connectToTarget(io_service, host, port);

            log("Connected to target server");

            // Запускаем передачу запросов между клиентом и сервером
            co_await (forwardData(client_socket, target_socket, "Client→Target") ||
                      forwardData(target_socket, client_socket, "Target→Client"));
            log("Session completed");
        } catch (const boost::system::system_error &e) {
            if (e.code() == boost::asio::error::eof) {
                log("Client closed connection gracefully");
            } else {
                throw;
            }
        }
    } catch (const std::exception &e) {
        log(std::format("Error: {}", e.what()));
    }
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), socket_(io_service) {
        do_accept();
    }

private:
    void do_accept() {
        std::println("Waiting for connection...");
        acceptor_.async_accept(socket_, [this](error_code ec) {
            co_spawn(acceptor_.get_executor(), session(std::move(socket_), io_service_), detached);
            do_accept();
        });
    }

    io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char *argv[]) {
    std::println("hello");
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy_server";
            std::cerr << " <listen_port>\n";
            return 1;
        }
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
