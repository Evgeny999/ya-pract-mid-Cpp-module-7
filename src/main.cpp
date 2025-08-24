#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
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
using boost::asio::ip::tcp;
using boost::system::error_code;

constexpr std::string_view delimiter = "\r\n\r\n";

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {
    std::string client_id = "unknown";
    auto log = [&client_id](std::string_view msg) { std::println("[{}] {}.", client_id, msg); };
    try {
        std::string buf;
        client_id = std::format("{}:{}", client_socket.remote_endpoint().address().to_string(),
                                client_socket.remote_endpoint().port());
        log("Client connected");
        try {
            for (;;) {
                std::size_t n = co_await async_read_until(client_socket, dynamic_buffer(buf), '\n', use_awaitable);
                co_await async_write(client_socket, buffer(buf, n), use_awaitable);
                buf.erase(0, n);

                log(std::format("Echoed {} bytes to client", n));
            }
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
