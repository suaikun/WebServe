#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

class http_conn {
public:
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 2048;

    http_conn() {}
    ~http_conn() {}

    void init(int sockfd, const sockaddr_in& addr, int trigMode);
    void close_conn();
    void process(); // 处理业务逻辑（解析请求并准备响应）
    bool read();    // 非阻塞读
    bool write();   // 非阻塞写

    static int m_epollfd;    // 所有的 socket 都在同一个 epoll 实例中
    static int m_user_count; // 统计在线人数

private:
    int m_sockfd;
    sockaddr_in m_address;
    int m_trigMode; // 触发模式 (0: LT, 1: ET)

    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx; 

    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx; 
    int m_bytes_have_send; 
};
#endif