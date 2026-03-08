# HTTP 模块

WebServer-v4 中的 HTTP 连接封装模块，负责 HTTP 协议的解析、请求业务处理以及响应的生成与发送。本模块基于**有限状态机 (FSM)** 实现 HTTP/1.1 解析，并采用 **mmap** 与 **writev** 实现高性能文件传输。

---

## 文件说明

| 文件 | 说明 |
| :--- | :--- |
| `http_conn.h` | HTTP 连接类定义，状态枚举及成员声明 |
| `http_conn.cpp` | 解析逻辑、业务路由、响应填充及 I/O 实现 |

---

## 核心功能

### 1. 有限状态机解析

- **主状态机**：依次处理请求行 → 请求头 → 请求体 (若存在)
- **从状态机**：按行解析 (`\r\n` 断句)，返回 `LINE_OK` / `LINE_BAD` / `LINE_OPEN`
- 支持 **GET**、**POST** 请求

### 2. 业务路由

| 请求 | 行为 |
| :--- | :--- |
| `GET /` | 返回 `login.html` 登录页 |
| `POST /login` | 表单校验：`user=admin&password=123` 通过则返回 `welcome.html`，否则 `error.html` |
| 其他 GET | 静态资源请求，根据 URL 映射到 `resources/` 目录下的文件 |

### 3. 高性能响应发送

- 使用 **mmap** 将目标文件映射到内存，避免多余拷贝
- 使用 **writev** 将响应头与文件内容一次性发送，减少系统调用次数
- 支持大文件分块发送时的 iovec 偏移更新

---

## 类接口

### 主要方法

| 方法 | 说明 |
| :--- | :--- |
| `init(int sockfd, const sockaddr_in& addr, int trigMode)` | 初始化连接，注册到 epoll |
| `read()` | 非阻塞读取请求数据 |
| `write()` | 发送响应（writev 集中写） |
| `process()` | 线程池工作入口：解析 → 路由 → 生成响应 |
| `close_conn(bool real_close)` | 关闭连接并从 epoll 移除 |

### 关键枚举

- **METHOD**：GET、POST 等
- **CHECK_STATE**：`CHECK_STATE_REQUESTLINE` / `CHECK_STATE_HEADER` / `CHECK_STATE_CONTENT`
- **HTTP_CODE**：`NO_REQUEST`、`GET_REQUEST`、`BAD_REQUEST`、`FILE_REQUEST` 等

---

## 配置参数

| 常量 | 值 | 说明 |
| :--- | :--- | :--- |
| `READ_BUFFER_SIZE` | 2048 | 读缓冲区大小 (字节) |
| `WRITE_BUFFER_SIZE` | 1024 | 写缓冲区大小 (字节) |
| `FILENAME_LEN` | 200 | 文件路径最大长度 |
| `doc_root` | `./resources` | 静态资源根目录 |

---

## 数据流

```
recv() 读入 → process_read() 解析 → do_request() 路由+mmap → process_write() 填充响应
                                                                    ↓
Client ← writev() 发送 ← m_iv[0]=响应头 + m_iv[1]=文件内容
```

---

## 依赖

- Linux 系统调用：`epoll`、`mmap`、`writev`、`fcntl`、`recv`
- 需在 `webserver.cpp` 中正确设置 `m_epollfd` 静态成员
- 静态资源目录 `resources/` 需存在对应 HTML 文件
