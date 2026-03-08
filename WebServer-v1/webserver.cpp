#include "webserver.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

WebServer::WebServer() : m_port(9006) {}
WebServer::~WebServer() = default;

void WebServer::init(int port) {
    m_port = port;
}

void WebServer::run_loop() {
    int listenfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return;
    }

    int opt = 1;
    ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(m_port));

    if (::bind(listenfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << "\n";
        ::close(listenfd);
        return;
    }

    if (::listen(listenfd, 128) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << "\n";
        ::close(listenfd);
        return;
    }

    std::cout << "listening on port " << m_port << " ...\n";

    while (true) {
        sockaddr_in cli{};
        socklen_t clilen = sizeof(cli);
        int connfd = ::accept(listenfd, reinterpret_cast<sockaddr*>(&cli), &clilen);
        if (connfd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << "\n";
            break;
        }

        char buf[4096];
        std::memset(buf, 0, sizeof(buf));
        ssize_t n = ::recv(connfd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            std::cout << "=== received request ===\n";
            std::cout << buf << "\n";
        } else if (n == 0) {
            std::cout << "client closed connection before sending data\n";
        } else {
            std::cerr << "recv failed: " << std::strerror(errno) << "\n";
        }

        const char body[] = "Hello, Tiny!\n";
        const std::string resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(sizeof(body) - 1) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + std::string(body);

        ::send(connfd, resp.data(), resp.size(), 0);
        ::close(connfd);
    }

    ::close(listenfd);
}

