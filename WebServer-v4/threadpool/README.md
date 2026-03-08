# 线程池模块

WebServer-v4 的**模板线程池**，负责管理固定数量的工作线程，从任务队列中竞争获取任务并执行。实现主线程（生产者）与工作线程（消费者）的解耦，配合 Proactor 模型，主线程专注 I/O，工作线程专注业务逻辑（如 HTTP 解析）。

---

## 文件说明

| 文件 | 说明 |
| :--- | :--- |
| `threadpool.h` | 模板类实现，声明与定义均在此文件（模板需在头文件中实例化） |

---

## 核心功能

### 1. 生产者-消费者模型

- **主线程**：调用 `append()` 将任务指针放入队列，唤醒一个空闲工作线程
- **工作线程**：阻塞等待任务，获取后调用 `request->process()` 执行业务
- **队列满**：`append()` 返回 `false`，拒绝新任务（防止内存溢出）

### 2. 模板设计

- `ThreadPool<T>`：T 为任务类型，本项目中为 `http_conn`
- **约束**：T 必须提供 `void process()`  public 方法

### 3. 优雅退出

- 析构时设置 `m_stop`，`notify_all()` 唤醒所有线程
- 所有工作线程 `join()` 后再析构，避免悬空引用

---

## 类接口

### ThreadPool\<T\>

| 方法 | 说明 |
| :--- | :--- |
| `ThreadPool(thread_number, max_requests)` | 构造：创建 thread_number 个工作线程，队列容量 max_requests |
| `~ThreadPool()` | 析构：通知停止并等待所有线程结束 |
| `append(T* request)` | 将任务加入队列，成功返回 true，队列满返回 false |

---

## 配置参数

| 参数 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `thread_number` | 8 | 工作线程数量 |
| `max_requests` | 10000 | 任务队列最大长度 |

本项目在 `webserver.cpp` 中实例化：`ThreadPool<http_conn>(m_thread_num, 10000)`，线程数由命令行 `-t` 指定。

---

## 数据流

```
主线程 (epoll)                        任务队列                        工作线程
    │                                    │                                │
    │  append(http_conn*)                 │                                │
    │ ──────────> m_workqueue ──────> run() 竞争获取 ──> process() 解析 HTTP
    │     notify_one()                    │                                │
```

---

## 依赖

- C++11：`std::thread`、`std::mutex`、`std::condition_variable`、`std::unique_lock`
- 标准库：`std::list`、`std::vector`
