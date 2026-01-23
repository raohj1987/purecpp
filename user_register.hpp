#pragma once

#include "common.hpp"
#include "email_verify.hpp"
#include "user_aspects.hpp"
#include <cinatra/smtp_client.hpp>
#include <openssl/sha.h>
#include <regex>

using namespace cinatra;

namespace purecpp {
inline std::string sha256_simple(std::string_view input) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(input.data()), input.size(),
         hash);

  std::string hex(SHA256_DIGEST_LENGTH * 2, '\0');

  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    char buf[2];

    // 高4位
    std::to_chars(buf, buf + 1, hash[i] >> 4, 16);
    hex[i * 2] = buf[0];

    // 低4位
    std::to_chars(buf, buf + 1, hash[i] & 0x0F, 16);
    hex[i * 2 + 1] = buf[0];
  }

  return hex;
}

class user_register_t {
public:
  // 处理用户注册请求（改为异步方法）
  async_simple::coro::Lazy<void> handle_register(coro_http_request &req,
                                                 coro_http_response &resp) {
    register_info info = std::any_cast<register_info>(req.get_user_data());
    const auto &cfg = purecpp_config::get_instance().user_cfg_;

    // save to database
    users_t user{.id = 0,
                 .status = STATUS_OF_OFFLINE.data(),
                 .is_verifyed = EmailVerifyStatus::UNVERIFIED,
                 .created_at = get_timestamp_milliseconds(),
                 .last_active_at = 0,
                 .avatar = cfg.default_avatar_url.data()};
    std::string pwd_sha = sha256_simple(info.password);
    user.pwd_hash = pwd_sha;
    std::copy(info.username.begin(), info.username.end(),
              user.user_name.begin());
    std::copy(info.email.begin(), info.email.end(), user.email.begin());

    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      co_return;
    }
    uint64_t id = conn->get_insert_id_after_insert(user);

    if (id == 0) {
      auto err = conn->get_last_error();
      CINATRA_LOG_ERROR << err;
      resp.set_status_and_content(status_type::bad_request, make_error(err));
      co_return;
    }

    // 注册成功后，创建邮箱验证token
    auto [token_created, token] =
        email_verify_t::create_verify_token(id, info.email);

    if (!token_created) {
      CINATRA_LOG_ERROR << "创建邮箱验证token失败";
      resp.set_status_and_content(
          status_type::internal_server_error,
          make_error("注册成功，但发送验证邮件失败，请稍后手动验证"));
      co_return;
    }

    // 发送邮箱验证邮件
    bool email_result =
        co_await email_verify_t::send_verify_email(info.email, token);

    if (!email_result) {
      CINATRA_LOG_ERROR << "发送验证邮件失败";
      // 即使邮件发送失败，也返回注册成功，因为用户已创建成功
      std::string json = make_data(
          user_resp_data{id, info.username, info.email, user.is_verifyed},
          "注册成功！请前往邮箱验证账号（如果未收到邮件，请检查垃圾邮件夹或重新"
          "发送验证邮件）");
      resp.set_status_and_content(status_type::ok, std::move(json));
      co_return;
    }

    std::string json = make_data(
        user_resp_data{id, info.username, info.email, user.is_verifyed},
        "注册成功！请前往邮箱验证账号。");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  // 处理邮箱验证请求
  static void handle_verify_email(coro_http_request &req,
                                  coro_http_response &resp) {
    verify_email_info info =
        std::any_cast<verify_email_info>(req.get_user_data());
    // 验证token
    bool valid = email_verify_t::verify_email_token(info.token);
    if (!valid) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效或过期的token"));
      return;
    }

    // 获取token对应的用户ID
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    auto users_token = conn->select(ormpp::all)
                           .from<users_token_t>()
                           .where(col(&users_token_t::token).param() &&
                                  col(&users_token_t::token_type).param())
                           .collect(info.token, TokenType::VERIFY_EMAIL);
    if (users_token.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("token不存在"));
      return;
    }
    auto user_id = users_token[0].user_id;
    // 查询用户是否存在
    auto users = conn->select(ormpp::all)
                     .from<users_t>()
                     .where(col(&users_t::id).param())
                     .collect(user_id);
    if (users.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("用户不存在"));
      return;
    }
    auto user = users[0];
    // 更新用户状态为已验证
    user.is_verifyed = EmailVerifyStatus::VERIFIED;
    conn->update(user);

    // 返回成功响应
    std::string json = make_data(true, "邮箱验证成功！");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  // 处理重新发送验证邮件请求
  static async_simple::coro::Lazy<void>
  handle_resend_verify_email(coro_http_request &req, coro_http_response &resp) {
    resend_verify_email_info info =
        std::any_cast<resend_verify_email_info>(req.get_user_data());

    // 查询数据库中是否已存在该邮箱的用户
    auto conn = connection_pool<dbng<mysql>>::instance().get();

    auto users = conn->select(ormpp::all)
                     .from<users_t>()
                     .where(col(&users_t::email).param())
                     .collect(info.email);

    if (users.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("邮箱不存在"));
      co_return;
    }

    users_t user = users[0];

    // 检查是否已经验证（使用正确的字段名）
    if (user.is_verifyed) {
      resp.set_status_and_content(status_type::ok,
                                  make_error("该邮箱已经验证"));
      co_return;
    }

    // 注册成功后，创建邮箱验证token
    auto [token_created, token] =
        email_verify_t::create_verify_token(user.id, info.email);

    if (!token_created) {
      CINATRA_LOG_ERROR << "创建邮箱验证token失败";
      resp.set_status_and_content(status_type::internal_server_error,
                                  make_error("发送邮件失败，请检查邮箱地址!"));
      co_return;
    }

    // 发送邮箱验证邮件
    bool email_result =
        co_await email_verify_t::send_verify_email(info.email, token);

    if (!email_result) {
      CINATRA_LOG_ERROR << "发送验证邮件失败";
      resp.set_status_and_content(status_type::internal_server_error,
                                  make_error("发送邮件失败，请检查邮箱地址!"));
      co_return;
    }

    // 返回成功响应
    std::string json =
        make_data(empty_data{}, "验证邮件已发送，请检查您的邮箱");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp