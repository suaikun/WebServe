#include "http_conn.h"

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found.\n";

// 网站根目录，请确保你的目录下有 resources 文件夹
const char* doc_root = "./resources";

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

// 工具函数声明
// --- epoll 工具函数完整实现 ---

// 1. 设置文件描述符非阻塞
void setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

// 2. 将文件描述符注册到 epoll 内核事件表
void addfd(int epollfd, int fd, bool one_shot, int trigMode) {
    epoll_event event;
    event.data.fd = fd;
    if (trigMode == 1) // ET 模式
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else               // LT 模式
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot) event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 3. 从 epoll 中删除文件描述符，并关闭该 socket
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 4. 修改 epoll 中文件描述符的监听事件，重置 EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int trigMode) {
    epoll_event event;
    event.data.fd = fd;
    if (trigMode == 1) // ET 模式
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else               // LT 模式
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http_conn::init(int sockfd, const sockaddr_in& addr, int trigMode) {
    m_sockfd = sockfd;
    m_address = addr;
    m_trigMode = trigMode;
    addfd(m_epollfd, sockfd, true, m_trigMode);
    m_user_count++;
    init();
}

void http_conn::init() {
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_content_length = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_string = 0; // 初始化指针
    bytes_have_send = 0;
    bytes_to_send = 0;
    //memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    //memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    //memset(m_real_file, '\0', FILENAME_LEN);
}

void http_conn::close_conn(bool real_close) {
    if (real_close && (m_sockfd != -1)) {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

// --- 从状态机：断句 ---
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r') {
            if ((m_checked_idx + 1) == m_read_idx) return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if ((m_checked_idx > 0) && (m_read_buf[m_checked_idx - 1] == '\r')) {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// --- 主状态机：解析逻辑 ---
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) 
           || ((line_status = parse_line()) == LINE_OK)) {
        text = get_line();
        m_start_line = m_checked_idx;
        
        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE:
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST) return BAD_REQUEST;
                break;
            case CHECK_STATE_HEADER:
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) return BAD_REQUEST;
                else if (ret == GET_REQUEST) return do_request();
                break;
            case CHECK_STATE_CONTENT:
                ret = parse_content(text);
                if (ret == GET_REQUEST) return do_request();
                line_status = LINE_OPEN;
                break;
            default: return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

// --- 解析第一行：支持 GET/POST ---
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    m_url = strpbrk(text, " \t");
    if (!m_url) return BAD_REQUEST;
    *m_url++ = '\0';

    char* method = text;
    if (strcasecmp(method, "GET") == 0) m_method = GET;
    else if (strcasecmp(method, "POST") == 0) m_method = POST; 
    else return BAD_REQUEST;

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version) return BAD_REQUEST;
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") != 0) return BAD_REQUEST;

    if (!m_url || m_url[0] != '/') return BAD_REQUEST;
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
    if (text[0] == '\0') {
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) m_linger = true;
    } else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char* text) {
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';
        m_string = text; //把请求正文的指针保存下来！
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// --- 核心业务：mmap 文件映射 ---
http_conn::HTTP_CODE http_conn::do_request() {
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    // 1. 如果用户访问根目录 "/", 默认路由到登录页面 (处理 GET)
    if (strcmp(m_url, "/") == 0) {
        strncpy(m_real_file + len, "/login.html", FILENAME_LEN - len - 1);
    } 
    // 2. 核心业务：拦截表单提交的 POST 请求 (处理 POST)
    else if (m_method == POST && strcmp(m_url, "/login") == 0) {
        if (m_string != nullptr) {
            // 只有当表单数据完全等于 "user=admin&password=123" 时才放行
            if (strcmp(m_string, "user=admin&password=123") == 0) {
                strncpy(m_real_file + len, "/welcome.html", FILENAME_LEN - len - 1);
            } else {
                strncpy(m_real_file + len, "/error.html", FILENAME_LEN - len - 1);
            }
        }
    }
    // 3. 其他常规的 GET 静态资源请求（图片、其他网页等）
    else {
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    }

    if (stat(m_real_file, &m_file_stat) < 0) return NO_RESOURCE;
    if (!(m_file_stat.st_mode & S_IROTH)) return FORBIDDEN_REQUEST;
    if (S_ISDIR(m_file_stat.st_mode)) return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// --- 集中写：writev 逻辑 ---
bool http_conn::write() {
    int temp = 0;
    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigMode);
        init();
        return true;
    }

    while (1) {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1) {
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        
        // 处理 writev 的偏移，确保大文件发送完整
        if (bytes_have_send >= m_write_idx) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        } else {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_write_idx - bytes_have_send;
        }

        if (bytes_to_send <= 0) {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigMode);
            if (m_linger) {
                init();
                return true;
            } else return false;
        }
    }
}

// 辅助函数：拼凑响应报文
bool http_conn::add_response(const char* format, ...) {
    if (m_write_idx >= WRITE_BUFFER_SIZE) return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len) && 
           add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close") &&
           add_response("\r\n");
}

bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret) {
        case FILE_REQUEST:
            add_status_line(200, ok_200_title);
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        default: return false;
    }
}

// 线程池工作线程的唯一入口
void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trigMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret) close_conn();
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trigMode);
}

bool http_conn::read() {
    // ... 保留你之前实现的非阻塞 recv 逻辑即可 ...
    if (m_read_idx >= READ_BUFFER_SIZE) return false;
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return false;
        } else if (bytes_read == 0) return false;
        m_read_idx += bytes_read;
    }
    return true;
}