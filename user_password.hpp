#pragma once

#include "entity.hpp"
#include "send_email.hpp"
#include "user_aspects.hpp"
#include "user_register.hpp"

using namespace cinatra;

namespace purecpp {
// 读取配置文件的辅助函数
inline std::optional<smtp_config> load_smtp_config() {
  std::ifstream file("cfg/smtp_config.json", std::ios::in);
  if (!file.is_open()) {
    std::cerr << "无法打开配置文件" << std::endl;
    return std::nullopt;
  }

  std::string json(1024, ' ');
  file.read(json.data(), json.size());
  json.resize(file.gcount());  // 调整到实际读取的大小

  smtp_config conf;
  iguana::from_json(conf, json);
  return conf;
}

// 生成随机token的函数
inline std::string generate_reset_token() {
  // 使用当前时间和随机数生成token
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
  auto value = now_ms.time_since_epoch().count();

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 999999);

  std::stringstream ss;
  ss << std::hex << value << std::hex << dis(gen);
  return ss.str();
}

inline bool send_reset_email2(const std::string& email,
                              const std::string& token) {
  // 加载配置
  auto option_conf = load_smtp_config();
  if (option_conf) {
    std::cerr << "无法加载SMTP配置" << std::endl;
    return false;
  }
  auto conf = option_conf.value();

  // 检查必要的配置是否存在
  if (conf.smtp_host.empty() || conf.smtp_user.empty() ||
      conf.smtp_password.empty()) {
    std::cerr << "SMTP配置不完整" << std::endl;
    return false;
  }

  // 生成重置链接
  std::string reset_link =
      "http://localhost:3389/reset_password.html?token=" + token;
  try {
    // 配置参数（需替换为实际信息）
    std::string smtp_host = conf.smtp_host;
    int smtp_port = conf.smtp_port;  // QQ邮箱SMTPS端口465，STARTTLS端口587
    bool is_smtps = true;
    std::string username = conf.smtp_user;      // 发件人邮箱
    std::string password = conf.smtp_password;  // 邮箱授权码（非密码）
    std::string from = username;
    std::string to = email;  // 收件人邮箱
    std::string subject = "PureCpp密码重置";
    std::string email_text;
    email_text += "<html><body>";
    email_text += "<h3>密码重置请求</h3>";
    email_text += "<p>您请求重置您的PureCpp密码。请点击以下链接进行重置：</p>";
    email_text +=
        "<a href=\"" + reset_link + "\">" + reset_link + "</a><br/><br/>";
    email_text += "<p>如果您没有请求重置密码，请忽略此邮件。</p>";
    email_text += "<p>此链接有效期为1小时。</p>";
    email_text += "<p>感谢您使用PureCpp！</p>";
    email_text += "</body></html>";

    // 发送邮件
    bool result =
        purecpp::send_email(smtp_host, smtp_port, is_smtps, username, password,
                            from, to, subject, email_text, true);
  } catch (const std::exception& e) {
    std::cerr << "SMTP发送失败: " << e.what() << std::endl;
    return false;
  }
  return true;
}

// 发送真实邮件的函数（使用cinatra SMTP客户端）
inline bool send_reset_email(const std::string& email,
                             const std::string& token) {
  // 加载配置
  auto option_conf = load_smtp_config();
  if (option_conf) {
    std::cerr << "无法加载SMTP配置" << std::endl;
    return false;
  }
  auto conf = option_conf.value();
  // 检查必要的配置是否存在
  if (conf.smtp_host.empty() || conf.smtp_user.empty() ||
      conf.smtp_password.empty()) {
    std::cerr << "SMTP配置不完整" << std::endl;
    return false;
  }

  // 生成重置链接
  std::string reset_link = conf.reset_password_url + "?token=" + token;

  try {
    // 创建io_context
    asio::io_context io_context;

    // 创建SMTP客户端（使用SSL）
    cinatra::smtp::client<cinatra::SSL> client(io_context);

    // 设置服务器信息
    cinatra::smtp::email_server server_info;
    server_info.server = conf.smtp_host;
    server_info.port = std::to_string(conf.smtp_port);
    server_info.user = conf.smtp_user;
    server_info.password = conf.smtp_password;
    client.set_email_server(server_info);

    // 设置邮件内容
    cinatra::smtp::email_data email_data;
    email_data.from_email = conf.smtp_from_email;
    email_data.to_email.push_back(email);
    email_data.subject = "PureCpp密码重置";

    // 构建邮件正文
    std::string email_text;
    email_text += "<html><body>";
    email_text += "<h3>密码重置请求</h3>";
    email_text += "<p>您请求重置您的PureCpp密码。请点击以下链接进行重置：</p>";
    email_text +=
        "<a href=\"" + reset_link + "\">" + reset_link + "</a><br/><br/>";
    email_text += "<p>如果您没有请求重置密码，请忽略此邮件。</p>";
    email_text += "<p>此链接有效期为1小时。</p>";
    email_text += "<p>感谢您使用PureCpp！</p>";
    email_text += "</body></html>";

    email_data.text = email_text;
    client.set_email_data(email_data);

    // 发送邮件
    client.start();

    // 运行io_context
    io_context.run();

    std::cout << "邮件发送成功: " << email << std::endl;
    return true;
  } catch (const std::exception& e) {
    std::cerr << "发送邮件时发生异常: " << e.what() << std::endl;
    return false;
  }
}

class user_password_t {
 public:
  /**
   * @brief 处理用户修改密码请求
   *
   * @param req HTTP请求对象
   * @param resp HTTP响应对象
   */
  void handle_change_password(coro_http_request& req,
                              coro_http_response& resp) {
    change_password_info info =
        std::any_cast<change_password_info>(req.get_user_data());

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();

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
    if (user.pwd_hash != purecpp::sha256_simple(info.old_password)) {
      rest_response<std::string_view> data{false, "旧密码错误"};
      std::string json;
      iguana::to_json(data, json);
      resp.set_status_and_content(status_type::bad_request, std::move(json));
      return;
    }

    // 更新新密码
    user.pwd_hash = purecpp::sha256_simple(info.new_password);
    auto result = conn->update<users_t>(user, "id=" + std::to_string(user.id));

    if (result != 1) {
      rest_response<std::string_view> data{false, "修改密码失败"};
      std::string json;
      iguana::to_json(data, json);
      resp.set_status_and_content(status_type::internal_server_error,
                                  std::move(json));
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

  // 处理忘记密码请求
  void handle_forgot_password(coro_http_request& req,
                              coro_http_response& resp) {
    forgot_password_info info =
        std::any_cast<forgot_password_info>(req.get_user_data());

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();

    // 查找用户
    auto users = conn->query_s<users_t>("email = ?", info.email);
    if (users.empty()) {
      // 用户不存在，但为了安全，不告诉用户邮箱是否存在
      rest_response<std::string_view> data{true,
                                           "如果邮箱存在，重置链接已发送"};
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
    uint64_t expires_at = now + 3600000;  // 1小时 = 3600000毫秒

    // 保存token到数据库
    password_reset_tokens_t reset_token{.id = 0,
                                        .user_id = user.id,
                                        .created_at = now,
                                        .expires_at = expires_at};

    std::copy(token.begin(), token.end(), reset_token.token.begin());

    // 删除该用户之前的所有重置token
    std::string delete_sql =
        "DELETE FROM password_reset_tokens WHERE user_id = " +
        std::to_string(user.id);
    conn->execute(delete_sql);

    // 插入新的token
    uint64_t insert_id = conn->get_insert_id_after_insert(reset_token);
    if (insert_id == 0) {
      auto err = conn->get_last_error();
      std::cout << err << "\n";
      rest_response<std::string_view> data{false,
                                           "生成重置链接失败，请稍后重试"};
      std::string json;
      iguana::to_json(data, json);
      resp.set_status_and_content(status_type::internal_server_error,
                                  std::move(json));
      return;
    }

    // 发送重置邮件（使用新的send_email函数）
    if (!send_reset_email2(info.email, token)) {
      // 邮件发送失败，但为了安全，仍返回相同响应以防止邮箱枚举攻击
      std::cerr << "邮件发送失败: " << info.email << std::endl;
    }

    // 返回成功响应
    rest_response<std::string_view> data{
        true, "密码重置链接已发送,请检查您的邮箱并完成后续操作"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  // 处理密码重置请求
  void handle_reset_password(coro_http_request& req, coro_http_response& resp) {
    reset_password_info info =
        std::any_cast<reset_password_info>(req.get_user_data());

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();

    // 查找token
    auto tokens =
        conn->query_s<password_reset_tokens_t>("token = ?", info.token);
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
    user.login_attempts = 0;
    user.last_failed_login = 0;
    bool update_success =
        conn->update<users_t>(user, "id = " + std::to_string(user.id));
    if (!update_success) {
      auto err = conn->get_last_error();
      std::cout << err << "\n";
      rest_response<std::string_view> data{false, "重置密码失败，请稍后重试"};
      std::string json;
      iguana::to_json(data, json);
      resp.set_status_and_content(status_type::internal_server_error,
                                  std::move(json));
      return;
    }

    // 删除已使用的token
    std::string delete_sql = "DELETE FROM password_reset_tokens WHERE id = " +
                             std::to_string(reset_token.id);
    conn->execute(delete_sql);

    // 返回成功响应
    rest_response<std::string_view> data{true, "密码重置成功"};
    std::string json;
    iguana::to_json(data, json);
    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
}  // namespace purecpp