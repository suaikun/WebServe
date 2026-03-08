# 日志模块

WebServer-v4 中的**异步日志系统**，基于**生产者-消费者**模型，通过线程安全阻塞队列将日志落盘操作与业务线程解耦，避免写文件阻塞主流程，提高服务器并发性能。

---

## 文件说明

| 文件 | 说明 |
| :--- | :--- |
| `block_queue.h` | 线程安全阻塞队列（模板类），用于日志缓冲 |
| `log.h` | 日志单例类声明及宏定义 |
| `log.cpp` | 异步写日志逻辑及文件 I/O 实现 |

---

## 核心功能

### 1. 阻塞队列 (BlockQueue)

- **生产者-消费者**：多线程可并发 `push`，后台线程 `pop` 消费
- **满则等**：队列满时生产者阻塞，由消费者取出后唤醒
- **空则等**：队列空时消费者阻塞，由生产者写入后唤醒
- **关闭接口**：`close()` 唤醒所有等待线程，清空队列

### 2. 异步日志 (Log)

- **单例模式**：C++11 局部静态变量，线程安全且懒加载
- **异步写盘**：`write_log()` 仅将日志压入队列即返回，落盘由后台线程完成
- **日志格式**：`YYYY-MM-DD HH:MM:SS.μμμμμμ [level]: 用户消息`
- **级别**：`debug` / `info` / `warn` / `erro`
- **文件名**：`YYYY_MM_DD_Server.log`，按日期分文件

---

## 类接口

### BlockQueue\<T\>

| 方法 | 说明 |
| :--- | :--- |
| `push(const T& item)` | 生产者写入，队列满时阻塞 |
| `pop(T& item)` | 消费者取出，队列空时阻塞 |
| `close()` | 关闭队列，唤醒所有等待线程 |

### Log（单例）

| 方法 | 说明 |
| :--- | :--- |
| `getInstance()` | 获取单例指针 |
| `init(file_name, close_log, max_queue_size)` | 初始化：创建队列、启动后台线程、打开日志文件 |
| `write_log(level, format, ...)` | 按 printf 风格格式化并写入队列 |
| `flush()` | 强制刷新文件缓冲区到磁盘 |

### 宏

| 宏 | 级别 | 示例 |
| :--- | :--- | :--- |
| `LOG_DEBUG(format, ...)` | debug | `LOG_DEBUG("fd=%d", sockfd);` |
| `LOG_INFO(format, ...)` | info | `LOG_INFO("server start");` |
| `LOG_WARN(format, ...)` | warn | `LOG_WARN("queue full");` |
| `LOG_ERROR(format, ...)` | error | `LOG_ERROR("open failed");` |

---

## 数据流

```
业务线程 (生产者)                   阻塞队列                    后台线程 (消费者)
    │                                  │                              │
    │  write_log() 格式化日志           │                              │
    │ ──────────> push() ──────────>  BlockQueue  ─────────> pop()   │
    │     立即返回                      │                              │  fputs() 落盘
    │                                  │                              │
```

---

## 配置参数

| 参数 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `close_log` | 0 | 1 表示关闭日志，0 表示开启 |
| `max_queue_size` | 1000 | 阻塞队列最大容量 |
| 单条日志最大长度 | 2048 字节 | 超出部分截断 |

---

## 依赖

- C++11：`std::thread`、`std::mutex`、`std::condition_variable`、`std::unique_lock`
- 系统调用：`gettimeofday`、`localtime`、`fputs`、`fflush`
