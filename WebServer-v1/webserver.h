#pragma once

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port);
    void run_epoll(); 

private:
    int m_port;
};

