#include "http_conn.h"
#include <iostream>
#include <cstring>
#include <fcntl.h>

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

// 工具函数：设置非阻塞
void setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

// 工具函数：添加到 epoll
void addfd(int epollfd, int fd, bool one_shot, int trigMode) {
    epoll_event event;
    event.data.fd = fd;
    if (trigMode == 1) // 1 为 ET 模式
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else               // 0 为 LT 模式
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot) event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 工具函数：修改 epoll 监听事件
void modfd(int epollfd, int fd, int ev, int trigMode) {
    epoll_event event;
    event.data.fd = fd;
    if (trigMode == 1)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http_conn::init(int sockfd, const sockaddr_in& addr, int trigMode) {
    m_sockfd = sockfd;
    m_address = addr;
    m_trigMode = trigMode;
    
    addfd(m_epollfd, sockfd, true, m_trigMode);
    m_user_count++;

    m_read_idx = 0;
    m_write_idx = 0;
    m_bytes_have_send = 0;
    memset(m_read_buf, 0, READ_BUFFER_SIZE);
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
}

void http_conn::close_conn() {
    if (m_sockfd != -1) {
        epoll_ctl(m_epollfd, EPOLL_CTL_DEL, m_sockfd, 0);
        close(m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

bool http_conn::read() {
    if (m_read_idx >= READ_BUFFER_SIZE) return false;
    int bytes_read = 0;

    // 非阻塞读取
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 读完了
            return false; // 出错
        } else if (bytes_read == 0) {
            return false; // 客户端关闭
        }
        m_read_idx += bytes_read;
        if (m_trigMode == 0) break; // LT 模式只读一次即可
    }
    return true;
}

void http_conn::process() {
    std::cout << "=== received request ===\n" << m_read_buf << "\n";
    const char body[] = "Hello, WebServer!\n";
    std::string resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(sizeof(body) - 1) + "\r\n"
        "Connection: close\r\n\r\n" + std::string(body);

    strcpy(m_write_buf, resp.c_str());
    m_write_idx = resp.size();

    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigMode); // 注册写事件
}

bool http_conn::write() {
    int temp = 0;
    if (m_write_idx == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigMode);
        return true;
    }
    while (1) {
        temp = send(m_sockfd, m_write_buf + m_bytes_have_send, m_write_idx - m_bytes_have_send, 0);
        if (temp <= -1) {
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigMode);
                return true;
            }
            return false;
        }
        m_bytes_have_send += temp;
        if (m_bytes_have_send >= m_write_idx) return false; // 发送完返回 false，主循环会关闭连接
    }
}