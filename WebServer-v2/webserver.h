#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/epoll.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <signal.h>
#include "http_conn.h"

const int MAX_FD = 65536;           // 系统最大文件描述符数量 (用于开辟 users 数组)
const int MAX_EVENT_NUMBER = 10000; // epoll 最大能监听的事件数量

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port);
    void trig_mode();
    void eventListen();
    void eventLoop();
    bool dealclientdata();
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

private:

    int m_port;

    //管道相关
    int m_pipefd[2];
    int m_epollfd;

    //用户相关
    http_conn *users;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];//epoll内核事件表，用于存储epoll_wait返回的事件

    int m_listenfd;//监听套接字
    int m_LISTENTrigmode;//监听触发模式
    int m_CONNTrigmode;//连接触发模式
};

#endif