# 定时器模块

WebServer-v4 中基于**最小堆 (Min-Heap)** 的高精度定时器，用于管理连接超时。结合 O(1) 哈希映射（`ref_[id]`），实现快速查找与续命，超时后执行回调（如关闭非活跃连接），并配合 `epoll_wait` 的超时参数实现定时轮询。

---

## 文件说明

| 文件 | 说明 |
| :--- | :--- |
| `timer.h` | TimerNode 结构体、HeapTimer 类声明 |
| `timer.cpp` | 堆操作、add/adjust/tick 等实现 |

---

## 核心功能

### 1. 最小堆

- **堆顶**：最先超时的节点
- **节点**：`(id, expires, cb)`，按 `expires` 排序
- **操作**：`siftup_` 插入、`siftdown_` 下沉、`swapNode_` 交换并同步 `ref_`

### 2. O(1) 查找与续命

- `ref_[65536]`：id（sockfd）→ 堆中下标，`-1` 表示不存在
- `adjust(id, timeout_ms)`：根据 id 直接定位并更新时间，仅需堆下沉，无需遍历

### 3. 与 epoll 配合

- `getNextTick()`：返回距下一个超时还有多少毫秒，供 `epoll_wait(timeout)` 使用
- 返回 `-1` 表示无定时器，`epoll_wait` 可一直阻塞
- `tick()` 在 `getNextTick()` 内部调用，先清理已超时节点再计算

---

## 类与结构体

### TimerNode

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `id` | int | 唯一标识，通常为 sockfd |
| `expires` | TimeStamp | 绝对超时时间点 |
| `cb` | TimeoutCallBack | 超时回调，如关闭连接 |

### HeapTimer

| 方法 | 说明 |
| :--- | :--- |
| `add(id, timeout_ms, cb)` | 添加或覆盖定时器，id 需 ∈ [0, 65535] |
| `adjust(id, timeout_ms)` | 续命：将 id 对应节点的超时时间延后 |
| `popNode(id)` | 删除指定定时器 |
| `tick()` | 移除所有已超时节点并依次执行其回调 |
| `getNextTick()` | 先 tick，再返回距下一个超时的时间 (ms)，空堆返回 -1 |
| `clear()` | 清空堆与映射表 |

---

## 在 WebServer 中的用法

| 时机 | 操作 | 说明 |
| :--- | :--- | :--- |
| accept 新连接 | `add(connfd, TIMEOUT_MS, CloseConn_)` | 注册 15 秒超时，回调关闭连接 |
| 读完请求数据 | `adjust(sockfd, TIMEOUT_MS)` | 续命 15 秒 |
| 写完响应数据 | `adjust(sockfd, TIMEOUT_MS)` | 长连接续命 |
| 连接关闭 | `popNode(sockfd)` | 从堆中移除，避免野指针 |
| epoll 循环 | `epoll_wait(..., getNextTick())` | 按最近超时时间阻塞，到期唤醒执行 tick |

---

## 时间精度

- 使用 `std::chrono::high_resolution_clock` 获取高精度时间
- 超时单位为毫秒 (ms)

---

## 依赖

- C++11：`std::chrono`、`std::function`、`std::vector`
- 标准库：`<algorithm>`（用于比较）
