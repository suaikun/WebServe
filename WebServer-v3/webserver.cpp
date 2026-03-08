#include "webserver.h"

// --- 全局信号处理辅助 ---
static int u_pipefd[2];

void sig_handler(int sig) {
    int save_errno = errno; // 保证函数可重入性
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void(handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
// 声明前面定义的辅助函数
extern void addfd(int epollfd, int fd, bool one_shot, int trigMode);
extern void setnonblocking(int fd);


WebServer::WebServer() {
    users = new http_conn[MAX_FD];
    m_pool = new ThreadPool<http_conn>(8, 10000);
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete m_pool;
}

void WebServer::init(int port) { m_port = port; }

void WebServer::trig_mode() {
    m_LISTENTrigmode = 0; // LT
    m_CONNTrigmode = 1;   // ET
}

void WebServer::eventListen() {
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    int opt = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    std::cout << "listening on port " << m_port << " ...\n";

    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    
    // 初始化 http_conn 静态变量
    http_conn::m_epollfd = m_epollfd;

    addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    setnonblocking(m_pipefd[1]);
    addfd(m_epollfd, m_pipefd[0], false, 0);

    // 设置全局管道，供信号处理函数使用
    u_pipefd[0] = m_pipefd[0];
    u_pipefd[1] = m_pipefd[1];

    addsig(SIGPIPE, SIG_IGN);
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
}

bool WebServer::dealclientdata() {
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address); // 修复了 socket_len_t 拼写错误
    int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
    if (connfd < 0) {
        std::cerr << "accept failed: " << std::strerror(errno) << "\n";
        return false;
    }
    if (http_conn::m_user_count >= MAX_FD) {
        std::cerr << "internal server busy\n";
        close(connfd); // 移除了不存在的 utils.show_error
        return false;
    }
    
    // 初始化新连接
    users[connfd].init(connfd, client_address, m_CONNTrigmode);
    return true;
}

void WebServer::dealwithread(int sockfd) {
    if (users[sockfd].read()) {
        m_pool->append(users+sockfd);
    } else {
        users[sockfd].close_conn(); // 读失败或对方断开，关闭连接
    }
}

void WebServer::dealwithwrite(int sockfd) {
    if (!users[sockfd].write()) {
        users[sockfd].close_conn(); // 写完后关闭（短连接）
    }
}

void WebServer::eventLoop() {
    bool stop_server = false;
    while (!stop_server) {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            std::cerr << "epoll failure: " << std::strerror(errno) << "\n";
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == m_listenfd) {
                bool flag = dealclientdata();
                if (false == flag) continue;
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                users[sockfd].close_conn();
            } else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
                char signals[1024];
                int ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0) continue;
                else {
                    for (int j = 0; j < ret; j++) {
                        if (signals[j] == SIGINT || signals[j] == SIGTERM) {
                            stop_server = true;
                            break;
                        }
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                dealwithread(sockfd);
            } else if (events[i].events & EPOLLOUT) {
                dealwithwrite(sockfd);
            }
        }
    }
}
