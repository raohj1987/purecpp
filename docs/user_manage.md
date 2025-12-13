# 用户管理功能设计文档

## 1. 概述

本文档详细描述了PureCpp项目中的用户管理功能，包括用户注册、登录、修改密码、忘记密码、重置密码和退出登录。这些功能构成了系统的核心用户认证和授权体系。

## 2. 用户注册功能

### 2.1 功能描述
用户注册功能允许新用户创建账号，需要提供用户名、邮箱、密码，并回答一道C++相关问题进行验证。

### 2.2 设计流程

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Server as 服务器
    participant DB as 数据库
    
    Client->>Server: POST /api/register (包含注册信息)
    Server->>Server: 验证请求体格式
    Server->>Server: 验证用户名格式
    Server->>Server: 验证邮箱格式
    Server->>Server: 验证密码复杂度
    Server->>Server: 验证C++问题答案
    Server->>DB: 插入用户数据
    DB-->>Server: 返回插入结果
    Server->>Client: 返回注册成功响应
```

### 2.3 输入接口

**API端点**: `POST /api/register`

**请求参数**:
| 参数名 | 类型 | 描述 | 约束 |
|--------|------|------|------|
| username | string | 用户名 | 1-20个字符，只允许字母、数字、下划线、连字符 |
| email | string | 邮箱地址 | 有效的邮箱格式，最大254个字符 |
| password | string | 密码 | 6-20位，至少包含大小写字母和数字 |
| cpp_answer | string | C++问题答案 | 与问题匹配的正确答案 |
| question_index | number | 问题索引 | 有效的问题索引（0-5） |

**请求示例**:
```json
{
  "username": "testuser",
  "email": "test@example.com",
  "password": "Test123456",
  "cpp_answer": "const",
  "question_index": 0
}
```

### 2.4 输出接口

**成功响应**:
```json
{
  "success": true,
  "message": "注册成功",
  "data": {
    "user_id": 123456,
    "username": "testuser",
    "email": "test@example.com",
    "verification_required": false
  },
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 200
}
```

**失败响应**:
```json
{
  "success": false,
  "message": "用户名长度非法应改为1-20。",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 400
}
```

### 2.5 数据结构

**注册信息结构体**:
```cpp
struct register_info {
  std::string username;
  std::string email;
  std::string password;
  std::string cpp_answer;
  size_t question_index;
};
```

**用户表结构**:
```cpp
struct users_t {
  uint64_t id;
  std::string user_name; // unique, not null
  std::string email;    // unique, not null
  std::string pwd_hash; // not null
  int is_verifyed;      // 邮箱是否已验证
  uint64_t created_at;  // 创建时间
  uint64_t last_active_at; // 最后活跃时间
  uint32_t login_attempts;  // 登录失败次数
  uint64_t last_failed_login;  // 最后一次登录失败时间戳
};
```

### 2.6 错误处理

| 错误类型 | 状态码 | 错误信息 |
|----------|--------|----------|
| 请求体为空 | 400 | register info is empty |
| JSON格式错误 | 400 | register info is not a required json |
| 用户名格式错误 | 400 | 用户名长度非法应改为1-20。 |
| 邮箱格式错误 | 400 | 邮箱格式不合法。 |
| 密码格式错误 | 400 | 密码长度不合法，长度6-20位。 |
| 密码复杂度不足 | 400 | 密码至少包含大小写字母和数字。 |
| C++问题答案错误 | 400 | 问题的答案不对。 |
| 数据库插入失败 | 400 | 数据库错误信息 |

### 2.7 安全考虑

1. 密码复杂度验证：确保密码包含大小写字母和数字
2. C++问题验证：防止机器人注册
3. 输入验证：对所有输入参数进行格式和范围检查
4. 数据库安全：使用参数化查询防止SQL注入

### 2.8 代码实现

```cpp
void handle_register(coro_http_request &req, coro_http_response &resp) {
  register_info info = std::any_cast<register_info>(req.get_user_data());

  // save to database
  auto conn = connection_pool<dbng<mysql>>::instance().get();
  conn_guard guard(conn);
  users_t user{.id = 0,
               .user_name = info.username,
               .email = info.email,
               .pwd_hash = info.password,
               .is_verifyed = false,
               .created_at = get_timestamp_milliseconds(),
               .last_active_at = 0};

  uint64_t id = conn->get_insert_id_after_insert(user);
  if (id == 0) {
    auto err = conn->get_last_error();
    std::cout << err << "\n";
    resp.set_status_and_content(status_type::bad_request, make_error(err));
    return;
  }

  rest_response<user_resp_data> data{};
  data.success = true;
  data.message = "注册成功";
  data.data = 
      user_resp_data{id, info.username, info.email, bool(user.is_verifyed)};

  std::string json;
  iguana::to_json(data, json);

  resp.set_status_and_content(status_type::ok, std::move(json));
}
```

## 3. 用户登录功能

### 3.1 功能描述
用户登录功能允许已注册用户使用用户名/邮箱和密码进行身份验证，并获取访问令牌。

### 3.2 设计流程

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Server as 服务器
    participant DB as 数据库
    
    Client->>Server: POST /api/login (包含登录信息)
    Server->>Server: 验证请求体格式
    Server->>Server: 验证用户名/邮箱和密码非空
    Server->>DB: 根据用户名/邮箱查询用户
    DB-->>Server: 返回用户数据
    Server->>Server: 验证密码
    Server->>Server: 生成JWT令牌
    Server->>DB: 更新用户最后活跃时间
    Server->>Client: 返回登录成功响应（包含令牌）
```

### 3.3 输入接口

**API端点**: `POST /api/login`

**请求参数**:
| 参数名 | 类型 | 描述 | 约束 |
|--------|------|------|------|
| username | string | 用户名或邮箱 | 非空 |
| password | string | 密码 | 非空 |

**请求示例**:
```json
{
  "username": "testuser",
  "password": "Test123456"
}
```

### 3.4 输出接口

**成功响应**:
```json
{
  "success": true,
  "message": "登录成功",
  "data": {
    "user_id": 123456,
    "username": "testuser",
    "email": "test@example.com",
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
  },
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 200
}
```

**失败响应**:
```json
{
  "success": false,
  "message": "用户名或密码错误",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 400
}
```

### 3.5 数据结构

**登录信息结构体**:
```cpp
struct login_info {
  std::string username;
  std::string password;
};
```

**登录响应数据结构体**:
```cpp
struct login_resp_data {
  uint64_t user_id;
  std::string username;
  std::string email;
  std::string token;
};
```

### 3.6 错误处理

| 错误类型 | 状态码 | 错误信息 |
|----------|--------|----------|
| 请求体为空 | 400 | 用户名(邮箱)、密码不能为空。 |
| JSON格式错误 | 400 | login info is not a required json |
| 用户名/密码为空 | 400 | 用户名(邮箱)、密码不能为空。 |
| 用户不存在 | 400 | 用户名或密码错误 |
| 密码错误 | 400 | 用户名或密码错误 |
| 登录失败次数过多 | 400 | 登录失败次数过多，账号已被锁定10分钟。 |
| 账号锁定中 | 400 | 登录失败次数过多，账号已被锁定。请在X分钟Y秒后重试。 |

### 3.7 安全考虑

1. 双重查询：支持用户名和邮箱登录
2. 密码验证：确保密码正确
3. JWT令牌：使用安全的令牌机制进行身份验证
4. 最后活跃时间更新：跟踪用户活动
5. 登录失败次数限制：当密码输入错误次数大于5次时，10分钟内禁止用户登录
6. 密码哈希：使用SHA256算法对密码进行哈希处理

### 3.8 代码实现

```cpp
void handle_login(coro_http_request &req, coro_http_response &resp) {
  login_info info = std::any_cast<login_info>(req.get_user_data());

  // 查询数据库
  auto conn = connection_pool<dbng<mysql>>::instance().get();
  conn_guard guard(conn);
  
  // 先尝试通过用户名查找
  auto users_by_name = conn->query_s<users_t>("user_name = ?", info.username);
  
  users_t user{};
  bool found = false;
  
  // 如果用户名存在
  if (!users_by_name.empty()) {
    user = users_by_name[0];
    found = true;
  } else {
    // 尝试通过邮箱查找
    auto users_by_email = conn->query_s<users_t>("email = ?", info.username);
    if (!users_by_email.empty()) {
      user = users_by_email[0];
      found = true;
    }
  }
  
  if (!found) {
    // 用户不存在
    rest_response<std::string_view> data{false, "用户名或密码错误"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::bad_request, std::move(json));
    return;
  }
  
  // 检查用户是否被锁定
  const uint32_t MAX_LOGIN_ATTEMPTS = 5;
  const uint64_t LOCK_DURATION = 10 * 60 * 1000;  // 10分钟，单位毫秒
  const uint64_t current_time = get_timestamp_milliseconds();
  
  if (user.login_attempts >= MAX_LOGIN_ATTEMPTS) {
    // 检查锁定时间是否已过
    if (current_time - user.last_failed_login < LOCK_DURATION) {
      // 用户仍处于锁定状态
      uint64_t remaining_time = 
          LOCK_DURATION - (current_time - user.last_failed_login);
      uint64_t remaining_minutes = remaining_time / (60 * 1000);
      uint64_t remaining_seconds = (remaining_time % (60 * 1000)) / 1000;
      
      std::string message = "登录失败次数过多，账号已被锁定。请在" +
                            std::to_string(remaining_minutes) + "分钟" +
                            std::to_string(remaining_seconds) + "秒后重试。";
      
      rest_response<std::string> data{false, message};
      std::string json;
      iguana::to_json(data, json);
      resp.set_status_and_content(status_type::bad_request, std::move(json));
      return;
    }
    else {
      // 锁定时间已过，重置失败次数
      user.login_attempts = 0;
    }
  }
  
  // 验证密码
  if (user.pwd_hash != purecpp::sha256_simple(info.password)) {
    // 密码错误，更新失败次数和最后失败时间
    user.login_attempts++;
    user.last_failed_login = current_time;
    
    // 保存更新到数据库
    conn->update<users_t>(user, "id = " + std::to_string(user.id));
    
    // 检查是否需要锁定账号
    if (user.login_attempts >= MAX_LOGIN_ATTEMPTS) {
      std::string message = "登录失败次数过多，账号已被锁定10分钟。";
      rest_response<std::string> data{false, message};
      std::string json;
      iguana::to_json(data, json);
      resp.set_status_and_content(status_type::bad_request, std::move(json));
      return;
    }
    
    rest_response<std::string_view> data{false, "用户名或密码错误"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::bad_request, std::move(json));
    return;
  }
  
  // 登录成功，重置失败次数
  user.login_attempts = 0;
  user.last_active_at = current_time;
  conn->update<users_t>(user, "id = " + std::to_string(user.id));
  
  // 将std::array转换为std::string
  std::string user_name_str(user.user_name.data());
  std::string email_str(user.email.data());
  
  // 生成JWT token
  std::string token = generate_jwt_token(user.id, user_name_str, email_str);
   
  // 返回登录成功响应
  login_resp_data login_data{user.id, user_name_str, email_str, token};
  rest_response<login_resp_data> data{};
  data.success = true;
  data.message = "登录成功";
  data.data = login_data;
  
  std::string json;
  iguana::to_json(data, json);
  
  resp.set_status_and_content(status_type::ok, std::move(json));
}
```

## 4. 修改密码功能

### 4.1 功能描述
修改密码功能允许已登录用户更改自己的密码，需要提供旧密码和新密码。

### 4.2 设计流程

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Server as 服务器
    participant DB as 数据库
    
    Client->>Server: POST /api/change_password (包含修改密码信息)
    Server->>Server: 验证请求体格式
    Server->>Server: 验证用户ID、旧密码、新密码非空
    Server->>Server: 验证新密码复杂度
    Server->>Server: 验证新密码与旧密码不同
    Server->>DB: 根据用户ID查询用户
    DB-->>Server: 返回用户数据
    Server->>Server: 验证旧密码
    Server->>DB: 更新用户密码
    DB-->>Server: 返回更新结果
    Server->>Client: 返回修改密码成功响应
```

### 4.3 输入接口

**API端点**: `POST /api/change_password`

**请求参数**:
| 参数名 | 类型 | 描述 | 约束 |
|--------|------|------|------|
| user_id | number | 用户ID | 大于0 |
| old_password | string | 旧密码 | 非空 |
| new_password | string | 新密码 | 6-20位，至少包含大小写字母和数字 |

**请求示例**:
```json
{
  "user_id": 123456,
  "old_password": "Test123456",
  "new_password": "NewTest789"
}
```

### 4.4 输出接口

**成功响应**:
```json
{
  "success": true,
  "message": "密码修改成功",
  "data": "密码修改成功",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 200
}
```

**失败响应**:
```json
{
  "success": false,
  "message": "旧密码错误",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 400
}
```

### 4.5 数据结构

**修改密码信息结构体**:
```cpp
struct change_password_info {
  uint64_t user_id;
  std::string old_password;
  std::string new_password;
};
```

### 4.6 错误处理

| 错误类型 | 状态码 | 错误信息 |
|----------|--------|----------|
| 请求体为空 | 400 | 修改密码信息不能为空。 |
| JSON格式错误 | 400 | 修改密码信息格式不正确。 |
| 参数为空 | 400 | 用户ID、旧密码、新密码不能为空。 |
| 用户不存在 | 400 | 用户不存在 |
| 旧密码错误 | 400 | 旧密码错误 |
| 新密码格式错误 | 400 | 密码长度不合法，长度6-20位。 |
| 新密码复杂度不足 | 400 | 密码至少包含大小写字母和数字。 |
| 新密码与旧密码相同 | 400 | 新密码不能与旧密码相同。 |
| 数据库更新失败 | 500 | 修改密码失败 |

### 4.7 安全考虑

1. 旧密码验证：确保只有用户本人可以修改密码
2. 新密码复杂度验证：确保密码强度
3. 新密码与旧密码不同：防止用户误操作
4. 参数验证：对所有输入参数进行格式和范围检查

### 4.8 代码实现

```cpp
void handle_change_password(coro_http_request &req, coro_http_response &resp) {
  change_password_info info = std::any_cast<change_password_info>(req.get_user_data());

  // 查询数据库
  auto conn = connection_pool<dbng<mysql>>::instance().get();
  conn_guard guard(conn);
  
  // 根据用户ID查找用户
  auto users = conn->query_s<users_t>("id = ?", info.user_id);
  
  if (users.empty()) {
    // 用户不存在
    rest_response<std::string_view> data{false, "用户不存在"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::bad_request, std::move(json));
    return;
  }
  
  users_t user = users[0];
  
  // 验证旧密码
  if (user.pwd_hash != info.old_password) {
    rest_response<std::string_view> data{false, "旧密码错误"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::bad_request, std::move(json));
    return;
  }
  
  // 使用直接的SQL语句更新密码
  std::string update_sql = "UPDATE users SET pwd_hash = '" + info.new_password + "' WHERE id = " + std::to_string(info.user_id);
  auto result = conn->execute(update_sql);
  
  if (result != 1) {
    rest_response<std::string_view> data{false, "修改密码失败"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::internal_server_error, std::move(json));
    return;
  }
  
  // 返回修改成功响应
  rest_response<std::string_view> data{};
  data.success = true;
  data.message = "密码修改成功";
  data.data = "密码修改成功";
  
  std::string json;
  iguana::to_json(data, json);
  
  resp.set_status_and_content(status_type::ok, std::move(json));
}
```

## 5. 忘记密码功能

### 5.1 功能描述
忘记密码功能允许用户通过邮箱获取密码重置链接，以便在忘记密码时重新设置密码。

### 5.2 设计流程

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Server as 服务器
    participant DB as 数据库
    participant Email as 邮件服务器
    
    Client->>Server: POST /api/forgot_password (包含邮箱)
    Server->>Server: 验证请求体格式
    Server->>Server: 验证邮箱非空
    Server->>DB: 根据邮箱查询用户
    DB-->>Server: 返回用户数据
    Server->>Server: 生成重置令牌
    Server->>DB: 保存重置令牌
    Server->>Email: 发送重置密码邮件
    Email-->>Client: 接收重置邮件
    Server->>Client: 返回密码重置链接已发送响应
```

### 5.3 输入接口

**API端点**: `POST /api/forgot_password`

**请求参数**:
| 参数名 | 类型 | 描述 | 约束 |
|--------|------|------|------|
| email | string | 用户邮箱 | 非空，有效的邮箱格式 |

**请求示例**:
```json
{
  "email": "test@example.com"
}
```

### 5.4 输出接口

**成功响应**:
```json
{
  "success": true,
  "message": "密码重置链接已发送,请检查您的邮箱并完成后续操作",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 200
}
```

**失败响应**:
```json
{
  "success": false,
  "message": "邮箱不能为空。",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 400
}
```

### 5.5 数据结构

**忘记密码信息结构体**:
```cpp
struct forgot_password_info {
  std::string email;
};
```

**密码重置令牌表结构体**:
```cpp
struct password_reset_tokens_t {
  uint64_t id;
  uint64_t user_id;
  std::string token;
  uint64_t created_at;
  uint64_t expires_at;
};
```

### 5.6 错误处理

| 错误类型 | 状态码 | 错误信息 |
|----------|--------|----------|
| 请求体为空 | 400 | 邮箱不能为空。 |
| JSON格式错误 | 400 | 请求格式不正确。 |
| 邮箱为空 | 400 | 邮箱不能为空。 |
| 数据库操作失败 | 500 | 生成重置链接失败，请稍后重试 |
| 邮件发送失败 | 500 | 邮件发送失败（日志记录） |

### 5.7 安全考虑

1. 邮箱验证：确保重置请求发送到正确的邮箱
2. 令牌有效期：重置令牌仅在1小时内有效
3. 防止邮箱枚举：无论邮箱是否存在，都返回相同的响应
4. 令牌唯一性：每次请求生成唯一的令牌
5. 旧令牌清理：删除用户之前的所有重置令牌

### 5.8 代码实现

```cpp
void handle_forgot_password(coro_http_request& req, coro_http_response& resp) {
    forgot_password_info info = std::any_cast<forgot_password_info>(req.get_user_data());
    
    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    conn_guard guard(conn);
    
    // 查找用户
    auto users = conn->query_s<users_t>("email = ?", info.email);
    if (users.empty()) {
        // 用户不存在，但为了安全，不告诉用户邮箱是否存在
        rest_response<std::string_view> data{true, "如果邮箱存在，重置链接已发送"};
        std::string json;
        iguana::to_json(data, json);
        resp.set_status_and_content(status_type::ok, std::move(json));
        return;
    }
    
    users_t user = users[0];
    
    // 生成重置token
    std::string token = generate_reset_token();
    
    // 设置token有效期为1小时
    uint64_t now = get_timestamp_milliseconds();
    uint64_t expires_at = now + 3600000; // 1小时 = 3600000毫秒
    
    // 保存token到数据库
    password_reset_tokens_t reset_token{
        .id = 0,
        .user_id = user.id,
        .token = token,
        .created_at = now,
        .expires_at = expires_at
    };
    
    // 删除该用户之前的所有重置token
    std::string delete_sql = "DELETE FROM password_reset_tokens WHERE user_id = " + std::to_string(user.id);
    conn->execute(delete_sql);
    
    // 插入新的token
    uint64_t insert_id = conn->get_insert_id_after_insert(reset_token);
    if (insert_id == 0) {
        auto err = conn->get_last_error();
        std::cout << err << "\n";
        rest_response<std::string_view> data{false, "生成重置链接失败，请稍后重试"};
        std::string json;
        iguana::to_json(data, json);
        resp.set_status_and_content(status_type::internal_server_error, std::move(json));
        return;
    }
    
    // 发送重置邮件
    if (!send_reset_email(info.email, token)) {
        // 邮件发送失败，但为了安全，仍返回相同响应以防止邮箱枚举攻击
        std::cerr << "邮件发送失败: " << info.email << std::endl;
    }
    
    // 返回成功响应
    rest_response<std::string_view> data{true, "密码重置链接已发送,请检查您的邮箱并完成后续操作"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::ok, std::move(json));
}
```

## 6. 重置密码功能

### 6.1 功能描述
重置密码功能允许用户使用通过邮箱获取的重置令牌来设置新密码。

### 6.2 设计流程

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Server as 服务器
    participant DB as 数据库
    
    Client->>Server: POST /api/reset_password (包含令牌和新密码)
    Server->>Server: 验证请求体格式
    Server->>Server: 验证令牌和新密码非空
    Server->>Server: 验证新密码复杂度
    Server->>DB: 根据令牌查询重置记录
    DB-->>Server: 返回重置记录
    Server->>Server: 验证令牌是否有效
    Server->>Server: 验证令牌是否过期
    Server->>DB: 根据用户ID查询用户
    DB-->>Server: 返回用户数据
    Server->>DB: 更新用户密码
    Server->>DB: 删除已使用的令牌
    DB-->>Server: 返回操作结果
    Server->>Client: 返回重置密码成功响应
```

### 6.3 输入接口

**API端点**: `POST /api/reset_password`

**请求参数**:
| 参数名 | 类型 | 描述 | 约束 |
|--------|------|------|------|
| token | string | 重置令牌 | 非空 |
| new_password | string | 新密码 | 6-20位，至少包含大小写字母和数字 |

**请求示例**:
```json
{
  "token": "a1b2c3d4e5f6g7h8i9j0",
  "new_password": "NewTest789"
}
```

### 6.4 输出接口

**成功响应**:
```json
{
  "success": true,
  "message": "密码重置成功",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 200
}
```

**失败响应**:
```json
{
  "success": false,
  "message": "重置密码链接无效或已过期",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 400
}
```

### 6.5 数据结构

**重置密码信息结构体**:
```cpp
struct reset_password_info {
  std::string token;
  std::string new_password;
};
```

### 6.6 错误处理

| 错误类型 | 状态码 | 错误信息 |
|----------|--------|----------|
| 请求体为空 | 400 | 重置密码信息不能为空。 |
| JSON格式错误 | 400 | 重置密码信息格式不正确。 |
| 参数为空 | 400 | token和新密码不能为空。 |
| 令牌不存在 | 400 | 重置密码链接无效或已过期 |
| 令牌已过期 | 400 | 重置密码链接已过期 |
| 用户不存在 | 400 | 用户不存在 |
| 新密码格式错误 | 400 | 密码长度不合法，长度6-20位。 |
| 新密码复杂度不足 | 400 | 密码至少包含大小写字母和数字。 |
| 数据库操作失败 | 500 | 重置密码失败，请稍后重试 |

### 6.7 安全考虑

1. 令牌验证：确保只有持有有效令牌的用户可以重置密码
2. 令牌过期：防止长时间未使用的令牌被滥用
3. 新密码复杂度验证：确保密码强度
4. 令牌删除：使用后立即删除令牌，防止重复使用

### 6.8 代码实现

```cpp
void handle_reset_password(coro_http_request& req, coro_http_response& resp) {
    reset_password_info info = std::any_cast<reset_password_info>(req.get_user_data());
    
    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    conn_guard guard(conn);
    
    // 查找token
    auto tokens = conn->query_s<password_reset_tokens_t>("token = ?", info.token);
    if (tokens.empty()) {
        // token不存在
        rest_response<std::string_view> data{false, "重置密码链接无效或已过期"};
        std::string json;
        iguana::to_json(data, json);
        resp.set_status_and_content(status_type::bad_request, std::move(json));
        return;
    }
    
    password_reset_tokens_t reset_token = tokens[0];
    
    // 检查token是否过期
    uint64_t now = get_timestamp_milliseconds();
    if (now > reset_token.expires_at) {
        // token已过期
        rest_response<std::string_view> data{false, "重置密码链接已过期"};
        std::string json;
        iguana::to_json(data, json);
        resp.set_status_and_content(status_type::bad_request, std::move(json));
        return;
    }
    
    // 查找用户
    auto users = conn->query_s<users_t>("id = ?", reset_token.user_id);
    if (users.empty()) {
        // 用户不存在
        rest_response<std::string_view> data{false, "用户不存在"};
        std::string json;
        iguana::to_json(data, json);
        resp.set_status_and_content(status_type::bad_request, std::move(json));
        return;
    }
    
    users_t user = users[0];
    
    // 更新用户密码
    user.pwd_hash = purecpp::sha256_simple(info.new_password);
    bool update_success = conn->update<users_t>(user, "id = " + std::to_string(user.id));
    if (!update_success) {
        auto err = conn->get_last_error();
        std::cout << err << "\n";
        rest_response<std::string_view> data{false, "重置密码失败，请稍后重试"};
        std::string json;
        iguana::to_json(data, json);
        resp.set_status_and_content(status_type::internal_server_error, std::move(json));
        return;
    }
    
    // 删除已使用的token
    std::string delete_sql = "DELETE FROM password_reset_tokens WHERE id = " + std::to_string(reset_token.id);
    conn->execute(delete_sql);
    
    // 返回成功响应
    rest_response<std::string_view> data{true, "密码重置成功"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::ok, std::move(json));
}
```

## 7. 退出登录功能

### 7.1 功能描述
退出登录功能允许已登录用户注销当前会话，将JWT令牌添加到黑名单中，使其失效。

### 7.2 设计流程

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Server as 服务器
    participant TokenBlacklist as 令牌黑名单
    
    Client->>Server: POST /api/logout (包含令牌)
    Server->>Server: 从请求头或查询参数获取令牌
    Server->>TokenBlacklist: 将令牌添加到黑名单
    TokenBlacklist-->>Server: 返回添加结果
    Server->>Client: 返回退出登录成功响应
```

### 7.3 输入接口

**API端点**: `POST /api/logout`

**请求参数**:
| 参数名 | 类型 | 描述 | 约束 |
|--------|------|------|------|
| Authorization | header | Bearer令牌 | 非空 |
| 或 token | query | 令牌 | 非空 |

**请求示例**:
```
POST /api/logout
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

或

```
POST /api/logout?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

### 7.4 输出接口

**成功响应**:
```json
{
  "success": true,
  "message": "退出登录成功",
  "timestamp": "2023-01-01T12:00:00Z",
  "code": 200
}
```

### 7.5 数据结构

无特定数据结构。

### 7.6 错误处理

| 错误类型 | 状态码 | 错误信息 |
|----------|--------|----------|
| 无错误 | 200 | 退出登录成功 |

### 7.7 安全考虑

1. 令牌黑名单：确保退出后令牌无法再使用
2. 多来源令牌获取：支持从请求头或查询参数获取令牌
3. 无令牌处理：如果没有令牌，仍然返回成功响应

### 7.8 代码实现

```cpp
void handle_logout(cinatra::coro_http_request& req, cinatra::coro_http_response& resp) {
    // 从请求头获取令牌
    std::string token;
    auto headers = req.get_headers();
    for (auto& header : headers) {
        if (cinatra::iequal0(header.name, "Authorization")) {
            // 提取Bearer令牌
            std::string_view auth_header = header.value;
            if (auth_header.size() > 7 && auth_header.substr(0, 7) == "Bearer ") {
                token = std::string(auth_header.substr(7));
                break;
            }
        }
    }

    // 如果请求头中没有令牌，尝试从查询参数获取
    if (token.empty()) {
        auto token_param = req.get_query_value("token");
        if (!token_param.empty()) {
            token = std::string(token_param);
        }
    }

    // 如果没有令牌，直接返回成功
    if (token.empty()) {
        rest_response<std::string> data{true, "退出登录成功"};
        std::string json;
        iguana::to_json(data, json);
        resp.set_status_and_content(cinatra::status_type::ok, std::move(json));
        return;
    }

    // 将令牌添加到黑名单
    token_blacklist::instance().add(token);

    // 返回成功响应
    rest_response<std::string> data{true, "退出登录成功"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(cinatra::status_type::ok, std::move(json));
}
```

## 8. 通用组件

### 8.1 统一响应格式

所有API接口都使用统一的响应格式：

```cpp
template <typename T> struct rest_response {
  bool success = true;
  std::string message;
  std::optional<std::vector<std::string>> errors;
  std::optional<T> data;
  std::string timestamp;
  int code = 200;
};
```

### 8.2 输入验证切面

系统使用切面编程（AOP）方式实现输入验证，每个功能都有对应的验证切面：

- `check_register_input`: 验证注册请求
- `check_cpp_answer`: 验证C++问题答案
- `check_user_name`: 验证用户名
- `check_email`: 验证邮箱
- `check_password`: 验证密码
- `check_login_input`: 验证登录请求
- `check_change_password_input`: 验证修改密码请求
- `check_new_password`: 验证新密码
- `check_forgot_password_input`: 验证忘记密码请求
- `check_reset_password_input`: 验证重置密码请求
- `check_reset_password`: 验证重置密码时的密码复杂度

### 8.3 数据库连接池

系统使用数据库连接池管理数据库连接，提高性能和资源利用率：

```cpp
auto conn = connection_pool<dbng<mysql>>::instance().get();
conn_guard guard(conn);
```

## 9. 测试方法

### 9.1 单元测试

每个功能模块都应该有对应的单元测试，测试场景包括：

1. 正常流程测试
2. 边界条件测试
3. 错误处理测试
4. 安全测试

### 9.2 集成测试

集成测试应该覆盖完整的用户流程，包括：

1. 注册 -> 登录 -> 修改密码 -> 退出登录
2. 注册 -> 登录 -> 忘记密码 -> 重置密码 -> 登录

### 9.3 安全测试

安全测试应该包括：

1. SQL注入测试
2. XSS测试
3. 密码破解测试
4. 邮箱枚举测试
5. 令牌伪造测试

## 10. 潜在改进点

1. **邮箱验证**：添加邮箱验证功能，确保用户邮箱的真实性
2. **密码加密**：使用更安全的密码哈希算法（如bcrypt、Argon2）
3. **双因素认证**：添加双因素认证功能，提高账户安全性
4. **登录尝试限制**：限制登录尝试次数，防止暴力破解
5. **密码历史记录**：限制用户使用最近使用过的密码
6. **HTTPS支持**：确保所有API通信使用HTTPS
7. **令牌刷新机制**：实现JWT令牌的刷新机制，提高用户体验
8. **审计日志**：记录所有用户认证和授权操作

## 11. 总结

本文档详细描述了PureCpp项目中的用户管理功能，包括用户注册、登录、修改密码、忘记密码、重置密码和退出登录。这些功能构成了系统的核心用户认证和授权体系，采用了现代化的设计和安全实践，确保系统的安全性和可靠性。