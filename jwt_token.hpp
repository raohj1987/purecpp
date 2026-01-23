#pragma once
#include "config.hpp"
#include <chrono>
#include <cinatra.hpp>
#include <mutex>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <optional>
#include <string>
#include <unordered_set>

namespace purecpp {

// Token校验结果结构体
enum class TokenValidationResult : int32_t {
  Valid,
  InvalidFormat,
  InvalidBase64,
  InvalidSignature,
  Expired
};

// Token信息结构体
struct access_token_info {
  uint64_t user_id;
  uint64_t iat; // 签发时间
  uint64_t exp; // 过期时间
};

// Token响应结构体，包含access token和refresh token
struct token_response {
  std::string access_token;
  std::string refresh_token;
  uint64_t access_token_expires_at;
  uint64_t refresh_token_expires_at;
  uint64_t access_token_lifetime; // 访问令牌有效期，单位：秒
};

// Refresh Token信息结构体
struct refresh_token_info {
  uint64_t user_id;
  uint64_t iat; // 签发时间
  uint64_t exp; // 过期时间
};

// HMAC-SHA1签名
std::string hmac_sha1(const std::string &data, const std::string &key) {
  unsigned char *hash = new unsigned char[EVP_MAX_MD_SIZE];
  unsigned int hash_len = 0;

  HMAC(EVP_sha1(), key.c_str(), key.size(),
       reinterpret_cast<const unsigned char *>(data.c_str()), data.size(), hash,
       &hash_len);

  std::string result;
  result.reserve(hash_len * 2);
  for (unsigned int i = 0; i < hash_len; i++) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02x", hash[i]);
    result.append(buf);
  }

  delete[] hash;
  return result;
}

// 生成简化JWT token的函数（无Header部分，仅包含Payload和Signature）
std::string generate_access_token(uint64_t user_id) {
  // 从配置文件中获取JWT密钥
  const std::string &jwt_secret =
      purecpp_config::get_instance().user_cfg_.access_token_secret;

  // 构建Payload，使用秒级时间戳
  uint64_t now = get_timestamp_seconds();
  // 从配置文件中获取过期时间（分钟），转换为秒
  uint64_t exp =
      now +
      static_cast<uint64_t>(
          purecpp_config::get_instance().user_cfg_.access_token_exp_minutes) *
          60;

  // 构建Payload JSON字符串，仅包含必要字段
  access_token_info t_token_info{user_id, now, exp};
  std::string payload;
  iguana::to_json(t_token_info, payload);

  std::string encoded_payload = cinatra::base64_encode(payload);

  // 构建Signature（仅对Payload进行签名，无Header），使用HMAC-SHA1
  std::string signature = hmac_sha1(encoded_payload, jwt_secret);
  std::string encoded_signature = cinatra::base64_encode(signature);

  // 构建简化的JWT（仅包含Payload和Signature，用点分隔）
  return encoded_payload + "." + encoded_signature;
}

// 生成refresh token的函数
std::string generate_refresh_token(uint64_t user_id) {
  // 从配置文件中获取refresh token密钥
  const std::string &refresh_token_secret =
      purecpp_config::get_instance().user_cfg_.refresh_token_secret;

  // 构建Payload，使用秒级时间戳
  uint64_t now = get_timestamp_seconds();
  // 从配置文件中获取过期时间（天），转换为秒
  uint64_t exp =
      now +
      static_cast<uint64_t>(
          purecpp_config::get_instance().user_cfg_.refresh_token_exp_days) *
          24 * 60 * 60;

  // 构建Payload JSON字符串，仅包含必要字段
  refresh_token_info t_refresh_token_info{user_id, now, exp};
  std::string payload;
  iguana::to_json(t_refresh_token_info, payload);

  std::string encoded_payload = cinatra::base64_encode(payload);

  // 构建Signature（仅对Payload进行签名，无Header），使用HMAC-SHA1
  std::string signature = hmac_sha1(encoded_payload, refresh_token_secret);
  std::string encoded_signature = cinatra::base64_encode(signature);

  // 构建refresh token（仅包含Payload和Signature，用点分隔）
  return encoded_payload + "." + encoded_signature;
}

// 生成包含access token和refresh token的token响应
token_response generate_jwt_token(uint64_t user_id, const std::string &username,
                                  const std::string &email) {
  // 生成access token
  std::string access_token = generate_access_token(user_id);

  // 生成refresh token
  std::string refresh_token = generate_refresh_token(user_id);

  // 计算过期时间，使用秒级时间戳
  uint64_t now = get_timestamp_seconds();

  uint64_t access_token_expires_at =
      now +
      static_cast<uint64_t>(
          purecpp_config::get_instance().user_cfg_.access_token_exp_minutes) *
          60;
  uint64_t refresh_token_expires_at =
      now +
      static_cast<uint64_t>(
          purecpp_config::get_instance().user_cfg_.refresh_token_exp_days) *
          24 * 60 * 60;

  // 计算access token有效期，单位：秒
  uint64_t access_token_lifetime =
      static_cast<uint64_t>(
          purecpp_config::get_instance().user_cfg_.access_token_exp_minutes) *
      60;

  // 构建token响应
  token_response response;
  response.access_token = access_token;
  response.refresh_token = refresh_token;
  response.access_token_expires_at = access_token_expires_at;
  response.refresh_token_expires_at = refresh_token_expires_at;
  response.access_token_lifetime = access_token_lifetime;

  return response;
}

// 令牌黑名单类
class token_blacklist {
public:
  // 获取单例实例
  static token_blacklist &instance() {
    static token_blacklist instance;
    return instance;
  }

  // 添加令牌到黑名单
  void add(const std::string &token) {
    std::lock_guard<std::mutex> lock(mutex_);
    blacklist_.insert(token);
  }

  // 检查令牌是否在黑名单中
  bool contains(const std::string &token) {
    std::lock_guard<std::mutex> lock(mutex_);
    return blacklist_.find(token) != blacklist_.end();
  }

private:
  // 私有构造函数
  token_blacklist() = default;
  // 禁用拷贝和赋值
  token_blacklist(const token_blacklist &) = delete;
  token_blacklist &operator=(const token_blacklist &) = delete;

  // 令牌黑名单
  std::unordered_set<std::string> blacklist_;
  // 互斥锁保护并发访问
  std::mutex mutex_;
};

// 验证refresh token
std::pair<TokenValidationResult, std::optional<refresh_token_info>>
validate_refresh_token(const std::string &token) {
  // 检查令牌是否在黑名单中
  if (token_blacklist::instance().contains(token)) {
    return {TokenValidationResult::Expired,
            std::nullopt}; // 使用Expired状态表示已注销
  }

  // 分割refresh token为Payload和Signature
  size_t dot_pos = token.find('.');

  if (dot_pos == std::string::npos) {
    return {TokenValidationResult::InvalidFormat, std::nullopt};
  }

  std::string encoded_payload = token.substr(0, dot_pos);
  std::string encoded_signature = token.substr(dot_pos + 1);

  // 解码Payload和Signature
  auto decoded_payload_opt = cinatra::base64_decode(encoded_payload);
  auto decoded_signature_opt = cinatra::base64_decode(encoded_signature);

  if (!decoded_payload_opt || !decoded_signature_opt) {
    return {TokenValidationResult::InvalidBase64, std::nullopt};
  }

  std::string decoded_payload = *decoded_payload_opt;
  std::string decoded_signature = *decoded_signature_opt;

  // 验证Signature，使用HMAC-SHA1
  const std::string &refresh_token_secret =
      purecpp_config::get_instance().user_cfg_.refresh_token_secret;
  std::string expected_signature =
      hmac_sha1(encoded_payload, refresh_token_secret);

  if (decoded_signature != expected_signature) {
    return {TokenValidationResult::InvalidSignature, std::nullopt};
  }

  // 解析Payload
  refresh_token_info info;
  std::error_code ec;
  iguana::from_json(info, decoded_payload, ec);
  if (ec) {
    return {TokenValidationResult::InvalidFormat, std::nullopt};
  }
  // 验证token是否过期，使用秒级时间戳
  uint64_t current_time =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  if (current_time > info.exp) {
    return {TokenValidationResult::Expired, std::nullopt};
  }
  return {TokenValidationResult::Valid, info};
}

// 使用refresh token刷新access token
token_response refresh_access_token(const std::string &refresh_token,
                                    uint64_t user_id) {
  // 验证refresh token
  auto [result, refresh_info_opt] = validate_refresh_token(refresh_token);

  if (result != TokenValidationResult::Valid || !refresh_info_opt) {
    throw std::runtime_error("Invalid refresh token");
  }

  const auto &refresh_info = *refresh_info_opt;

  // 校验user_id是否匹配
  if (refresh_info.user_id != user_id) {
    throw std::runtime_error("User ID mismatch");
  }

  // 生成新的access token
  std::string new_access_token = generate_access_token(refresh_info.user_id);

  // 计算过期时间，使用秒级时间戳
  uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();

  uint64_t access_token_expires_at =
      now +
      static_cast<uint64_t>(
          purecpp_config::get_instance().user_cfg_.access_token_exp_minutes) *
          60;

  // 计算access token有效期，单位：秒
  uint64_t access_token_lifetime =
      static_cast<uint64_t>(
          purecpp_config::get_instance().user_cfg_.access_token_exp_minutes) *
      60;

  // 构建token响应(保持refresh token有效期)
  token_response response;
  response.access_token = new_access_token;
  response.access_token_expires_at = access_token_expires_at;
  response.access_token_lifetime = access_token_lifetime;
  response.refresh_token = refresh_token;
  response.refresh_token_expires_at = refresh_info.exp;

  return response;
}

// Token校验函数
std::pair<TokenValidationResult, std::optional<access_token_info>>
validate_jwt_token(const std::string &token) {
  // 检查令牌是否在黑名单中
  if (token_blacklist::instance().contains(token)) {
    return {TokenValidationResult::Expired,
            std::nullopt}; // 使用Expired状态表示已注销
  }

  // 分割JWT，Payload.Signature
  size_t first_dot = token.find('.');
  if (first_dot == std::string::npos) {
    return {TokenValidationResult::InvalidFormat, std::nullopt};
  }

  std::string encoded_payload = token.substr(0, first_dot);
  std::string encoded_signature = token.substr(first_dot + 1);

  // 解码Payload和Signature
  auto decoded_payload_opt = cinatra::base64_decode(encoded_payload);
  auto decoded_signature_opt = cinatra::base64_decode(encoded_signature);

  if (!decoded_payload_opt || !decoded_signature_opt) {
    return {TokenValidationResult::InvalidBase64, std::nullopt};
  }

  std::string decoded_payload = *decoded_payload_opt;
  std::string decoded_signature = *decoded_signature_opt;

  // 从配置文件中获取JWT密钥
  const std::string &jwt_secret =
      purecpp_config::get_instance().user_cfg_.access_token_secret;
  // 校验Signature，使用HMAC-SHA1
  std::string expected_signature = hmac_sha1(encoded_payload, jwt_secret);
  if (decoded_signature != expected_signature) {
    return {TokenValidationResult::InvalidSignature, std::nullopt};
  }

  // 解析Payload
  access_token_info info;
  std::error_code ec;
  iguana::from_json(info, decoded_payload, ec);
  if (ec) {
    return {TokenValidationResult::InvalidFormat, std::nullopt};
  }
  // 验证token是否过期，使用秒级时间戳
  uint64_t current_time =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  if (current_time > info.exp) {
    return {TokenValidationResult::Expired, std::nullopt};
  }
  return {TokenValidationResult::Valid, info};
}

/**
 * @brief 从请求中提取用户ID
 * @param req HTTP请求
 * @return 用户ID
 */
uint64_t get_user_id_from_token(coro_http_request &req) {
  auto aspect_data = req.params_["user_token"];
  if (aspect_data.empty()) {
    return 0;
  }
  access_token_info token_info;
  std::error_code ec;
  iguana::from_json(token_info, aspect_data, ec);
  if (ec) {
    return 0;
  }
  return token_info.user_id;
}
} // namespace purecpp