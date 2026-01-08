#pragma once
#include <string>
#include <system_error>

#include "entity.hpp" // 包含必要的枚举类型

namespace purecpp {
// 注册相关结构体
struct register_info {
  std::string username;
  std::string email;
  std::string password;
  std::string cpp_answer;
  size_t question_index;
};

// 用户响应数据结构
struct user_resp_data {
  uint64_t user_id;
  std::string username;
  std::string email;
  int is_verifyed;
  UserTitle title;
  std::string role;
  uint64_t experience;
  UserLevel level;
};

// 登录相关结构体
struct login_info {
  std::string username;
  std::string password;
};

struct login_resp_data {
  uint64_t user_id;
  std::string username;
  std::string email;
  std::string token;
  std::string refresh_token;
  uint64_t access_token_expires_at;
  uint64_t refresh_token_expires_at;
  uint64_t access_token_lifetime; // 访问令牌有效期，单位：秒
  UserTitle title;
  std::string role;
  uint64_t experience;
  UserLevel level;
};

// 登出相关结构体
struct logout_info {
  uint64_t user_id;
};

// 修改密码相关结构体
struct change_password_info {
  uint64_t user_id;
  std::string old_password;
  std::string new_password;
};

// 忘记密码相关结构体
struct forgot_password_info {
  std::string email;
};

struct reset_password_info {
  std::string token;
  std::string new_password;
};

// 空数据结构体，用于没有具体数据的响应
struct empty_data {};

// 验证邮箱相关结构体
struct verify_email_info {
  std::string token;
};

struct resend_verify_email_info {
  std::string email;
};

// Refresh token请求结构体
struct refresh_token_request {
  std::string refresh_token;
  uint64_t user_id; // 新增：用户ID，用于校验
};

struct refresh_token_response {
  uint64_t user_id;
  std::string token;
  std::string refresh_token;
  uint64_t refresh_token_expires_at;
  uint64_t access_token_expires_at;
  uint64_t access_token_lifetime; // 访问令牌有效期，单位：秒
};
} // namespace purecpp