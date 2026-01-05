#pragma once

#include "common.hpp"
#include "config.hpp"
#include "entity.hpp"
#include "user_register.hpp"
#include <cinatra.hpp>
#include <cinatra/smtp_client.hpp>

using namespace cinatra;

namespace purecpp {
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

// 发送真实邮件的函数（使用cinatra SMTP客户端）
async_simple::coro::Lazy<bool> send_reset_email(const std::string &email,
                                                const std::string &token) {
  auto &conf = purecpp_config::get_instance();
  auto &user_conf = conf.user_cfg_;
  // 检查必要的配置是否存在
  if (user_conf.smtp_host.empty() || user_conf.smtp_user.empty() ||
      user_conf.smtp_password.empty()) {
    CINATRA_LOG_ERROR << "SMTP配置不完整";
    co_return false;
  }

  // 生成重置链接
  std::string reset_link = user_conf.reset_password_url + "?token=" + token;

  try {
    // 创建SMTP客户端（使用SSL）
    auto client = smtp::get_smtp_client(coro_io::get_global_executor());
    bool r = co_await client.connect(user_conf.smtp_host,
                                     std::to_string(user_conf.smtp_port));
    // 连接SMTP服务器
    if (!r) {
      CINATRA_LOG_ERROR << "SMTP连接失败";
      co_return false;
    }

    // 设置邮件内容
    cinatra::smtp::email_data email_data;
    email_data.user_name = user_conf.smtp_user;
    email_data.auth_pwd = user_conf.smtp_password;
    email_data.from_email = user_conf.smtp_from_email;
    email_data.to_email.push_back(email);
    email_data.subject = "PureCpp密码重置";

    email_data.is_html = true;

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
    r = co_await client.send_email(email_data);
    if (!r) {
      CINATRA_LOG_ERROR << "邮件发送失败: " << email;
      co_return false;
    }

    CINATRA_LOG_INFO << "邮件发送成功: " << email;
    co_return true;
  } catch (const std::exception &e) {
    CINATRA_LOG_ERROR << "发送邮件时发生异常: " << e.what();
    co_return false;
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
  void handle_change_password(coro_http_request &req,
                              coro_http_response &resp) {
    change_password_info info =
        std::any_cast<change_password_info>(req.get_user_data());

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();

    // 根据用户ID查找用户
    auto users = conn->select(ormpp::all)
                     .from<users_t>()
                     .where(col(&users_t::id).param())
                     .collect(info.user_id);

    if (users.empty()) {
      // 用户不存在
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("用户不存在"));
      return;
    }

    users_t user = users[0];

    // 验证旧密码
    if (user.pwd_hash != sha256_simple(info.old_password)) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("旧密码错误"));
      return;
    }

    // 更新新密码
    std::string pwd_sha = sha256_simple(info.new_password);
    user.pwd_hash = pwd_sha;
    if (conn->update<users_t>(user) != 1) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("修改密码失败"));
      return;
    }

    // 返回修改成功响应
    std::string json = make_data(empty_data{}, "密码修改成功");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  // 处理忘记密码请求
  async_simple::coro::Lazy<void>
  handle_forgot_password(coro_http_request &req, coro_http_response &resp) {
    forgot_password_info info =
        std::any_cast<forgot_password_info>(req.get_user_data());

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();

    // 查找用户
    auto users = conn->select(ormpp::all)
                     .from<users_t>()
                     .where(col(&users_t::email).param())
                     .collect(info.email);
    if (users.empty()) {
      resp.set_status_and_content(status_type::ok,
                                  make_error("如果邮箱存在，重置链接已发送"));
      co_return;
    }

    users_t user = users[0];

    // 生成重置token
    std::string token = generate_reset_token();

    // 设置token有效期为1小时
    uint64_t now = get_timestamp_milliseconds();
    uint64_t expires_at = now + 3600000; // 1小时 = 3600000毫秒

    // 保存token到数据库
    users_token_t reset_token{.id = 0,
                              .user_id = user.id,
                              .token_type = TokenType::RESET_PASSWORD,
                              .created_at = now,
                              .expires_at = expires_at};

    std::copy(token.begin(), token.end(), reset_token.token.begin());
    // 删除该用户之前的所有重置token
    std::string delete_sql =
        "DELETE FROM user_tokens WHERE user_id = " + std::to_string(user.id) +
        " AND token_type = " + std::to_string(int(TokenType::RESET_PASSWORD));
    conn->execute(delete_sql);

    // 插入新的token
    uint64_t insert_id = conn->get_insert_id_after_insert(reset_token);
    if (insert_id == 0) {
      auto err = conn->get_last_error();
      CINATRA_LOG_ERROR << err;
      resp.set_status_and_content(status_type::internal_server_error,
                                  make_error("生成重置链接失败，请稍后重试"));
      co_return;
    }

    // 发送重置邮件（使用新的send_email函数）
    bool r = co_await send_reset_email(info.email, token);
    if (!r) {
      // 邮件发送失败，返回错误信息
      CINATRA_LOG_ERROR << "邮件发送失败: " << info.email;
      resp.set_status_and_content(status_type::internal_server_error,
                                  make_error("发送邮件失败，请稍后重试"));
      co_return;
    }

    // 返回成功响应
    std::string json = make_data(
        empty_data{}, "密码重置链接已发送,请检查您的邮箱并完成后续操作");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  // 处理密码重置请求
  void handle_reset_password(coro_http_request &req, coro_http_response &resp) {
    reset_password_info info =
        std::any_cast<reset_password_info>(req.get_user_data());

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();

    // 查找token
    auto tokens = conn->select(ormpp::all)
                      .from<users_token_t>()
                      .where(col(&users_token_t::token).param())
                      .collect(info.token);
    if (tokens.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("重置密码链接无效或已过期"));
      return;
    }

    users_token_t reset_token = tokens[0];

    // 检查token是否过期
    uint64_t now = get_timestamp_milliseconds();
    if (now > reset_token.expires_at) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("重置密码链接已过期"));
      return;
    }

    // 查找用户
    auto users = conn->select(ormpp::all)
                     .from<users_t>()
                     .where(col(&users_t::id).param())
                     .collect(reset_token.user_id);
    if (users.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("用户不存在"));
      return;
    }

    users_t user = users[0];

    // 更新用户密码
    std::string pwd_hash = purecpp::sha256_simple(info.new_password);
    user.pwd_hash = pwd_hash;
    user.login_attempts = 0;
    user.last_failed_login = 0;
    if (conn->update<users_t>(user) != 1) {
      auto err = conn->get_last_error();
      CINATRA_LOG_ERROR << err;
      resp.set_status_and_content(status_type::internal_server_error,
                                  make_error("重置密码失败，请稍后重试"));
      return;
    }
    // 删除已使用的token
    // 删除该用户之前的所有重置token
    std::string delete_sql =
        "DELETE FROM user_tokens WHERE user_id = " + std::to_string(user.id) +
        " AND token_type = " + std::to_string(int(TokenType::RESET_PASSWORD));
    conn->execute(delete_sql);

    // 返回成功响应
    std::string json = make_data(empty_data{}, "密码重置成功");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp
