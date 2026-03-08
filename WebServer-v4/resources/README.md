# 静态资源目录

WebServer-v4 的 Web 根目录，存放服务器对外提供的 HTML 页面及静态资源。HTTP 模块会将请求 URL 映射到此目录下的文件（参见 `http/http_conn.cpp` 中的 `doc_root`）。

---

## 文件说明

| 文件 | 说明 |
| :--- | :--- |
| `login.html` | 登录页，GET `/` 时返回，包含账号密码表单 |
| `welcome.html` | 登录成功页，POST `/login` 校验通过后返回 |
| `error.html` | 登录失败页，POST `/login` 校验失败后返回 |

---

## 路由与资源映射

| 请求 | 返回资源 |
| :--- | :--- |
| `GET /` | `login.html` |
| `POST /login`（`user=admin&password=123`） | `welcome.html` |
| `POST /login`（其他账号密码） | `error.html` |
| `GET /xxx`（任意路径） | `resources/xxx` 对应文件（若存在） |

---

## 登录表单说明

- **表单字段**：`user`（账号）、`password`（密码）
- **提交方式**：POST，`application/x-www-form-urlencoded`
- **校验规则**：`user=admin&password=123` 时返回欢迎页，否则返回错误页
- **测试账号**：`admin` / `123`

---

## 图片资源

- `welcome.html` 引用 `w.jpg`（成功图）
- `error.html` 引用 `l.jpg`（失败图）

若需显示图片，请将 `w.jpg`、`l.jpg` 放入本目录；否则将返回 404。

---

## 扩展说明

- 可在此目录新增其他 HTML、CSS、JS、图片等静态文件
- 通过 `GET /文件名` 访问，例如 `GET /style.css`、`GET /logo.png`
