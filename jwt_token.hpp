#pragma once
#include "cinatra/utils.hpp"
#include "config.hpp"
#include <chrono>
#include <mutex>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>

namespace purecpp {
// HMAC-SHA256签名
std::string hmac_sha256(const std::string &data, const std::string &key) {
  unsigned char *hash = new unsigned char[EVP_MAX_MD_SIZE];
  unsigned int hash_len = 0;

  HMAC(EVP_sha256(), key.c_str(), key.size(),
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
auto generate_jwt_token(uint64_t user_id, const std::string &username,
                        const std::string &email) {
  // 从配置文件中获取JWT密钥
  const std::string &jwt_secret =
      purecpp_config::get_instance().user_cfg_.jwt_secret;

  // 构建Payload
  uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  // 从配置文件中获取过期时间（分钟），转换为毫秒
  uint64_t expiration_minutes =
      purecpp_config::get_instance().user_cfg_.token_expiration_minutes;
  uint64_t expiration_time = expiration_minutes * 60 * 1000;
  uint64_t exp = now + expiration_time;

  // 构建Payload JSON字符串，仅包含必要字段
  std::stringstream payload_ss;
  payload_ss << "{\"sub\":" << user_id << ",\"iat\":" << now
             << ",\"exp\":" << exp << "}";
  std::string payload = payload_ss.str();

  std::string encoded_payload = cinatra::base64_encode(payload);

  // 构建Signature（仅对Payload进行签名，无Header）
  std::string signature = hmac_sha256(encoded_payload, jwt_secret);
  std::string encoded_signature = cinatra::base64_encode(signature);

  // 构建简化的JWT（仅包含Payload和Signature，用点分隔）
  return encoded_payload + "." + encoded_signature;
}

// Token校验结果结构体
enum class TokenValidationResult : int32_t {
  Valid,
  InvalidFormat,
  InvalidBase64,
  InvalidSignature,
  Expired
};

// Token信息结构体
struct token_info {
  uint64_t user_id;
  uint64_t iat; // 签发时间
  uint64_t exp; // 过期时间
};

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

// 解析JSON字符串中的字段
std::optional<std::string> get_json_field(const std::string &json,
                                          const std::string &field) {
  size_t field_pos = json.find('"' + field + '"');
  if (field_pos == std::string::npos) {
    return std::nullopt;
  }

  size_t colon_pos = json.find(':', field_pos);
  if (colon_pos == std::string::npos) {
    return std::nullopt;
  }

  size_t value_start = json.find_first_not_of(" \t\n\r", colon_pos + 1);
  if (value_start == std::string::npos) {
    return std::nullopt;
  }

  size_t value_end;
  if (json[value_start] == '"') {
    // 字符串值
    value_start++;
    value_end = json.find('"', value_start);
    if (value_end == std::string::npos) {
      return std::nullopt;
    }
  } else {
    // 数字值
    value_end = json.find_first_of(",}\n\r", value_start);
    if (value_end == std::string::npos) {
      value_end = json.size();
    }
  }

  return json.substr(value_start, value_end - value_start);
}

// Token校验函数
std::pair<TokenValidationResult, std::optional<token_info>>
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
      purecpp_config::get_instance().user_cfg_.jwt_secret;
  // 校验Signature
  std::string expected_signature = hmac_sha256(encoded_payload, jwt_secret);
  if (decoded_signature != expected_signature) {
    return {TokenValidationResult::InvalidSignature, std::nullopt};
  }

  // 解析Payload
  token_info info;
  std::error_code ec;
  iguana::from_json(info, decoded_payload, ec);
  if (ec) {
    return {TokenValidationResult::InvalidFormat, std::nullopt};
  }
  // 验证token是否过期
  uint64_t current_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  if (current_time > info.exp) {
    return {TokenValidationResult::Expired, std::nullopt};
  }
  return {TokenValidationResult::Valid, info};
}

} // namespace purecpp