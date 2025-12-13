#pragma once
#include <chrono>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace purecpp {

// 本地实现的时间戳函数，避免依赖user_aspects.hpp
inline uint64_t get_jwt_timestamp_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// 生成JWT token的函数
auto generate_jwt_token(uint64_t user_id, const std::string &username,
                        const std::string &email) {
  // 简单的JWT生成，实际应用中应该使用更安全的实现
  // 这里使用base64编码的方式生成一个简单的token
  // 格式: user_id:username:email:timestamp

  std::string token = std::to_string(user_id) + ":" + username + ":" + email +
                      ":" + std::to_string(get_jwt_timestamp_milliseconds());

  // 简单的base64编码实现
  static const std::string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string result;
  result.reserve(((token.size() + 2) / 3) * 4);

  int i = 0;
  while (i < token.size()) {
    uint32_t octet_a = i < token.size() ? static_cast<uint8_t>(token[i++]) : 0;
    uint32_t octet_b = i < token.size() ? static_cast<uint8_t>(token[i++]) : 0;
    uint32_t octet_c = i < token.size() ? static_cast<uint8_t>(token[i++]) : 0;

    uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

    result += base64_chars[(triple >> 18) & 0x3F];
    result += base64_chars[(triple >> 12) & 0x3F];
    result += base64_chars[(triple >> 6) & 0x3F];
    result += base64_chars[triple & 0x3F];
  }

  // 处理填充
  if (token.size() % 3 == 1) {
    result[result.size() - 2] = '=';
    result[result.size() - 1] = '=';
  }
  else if (token.size() % 3 == 2) {
    result[result.size() - 1] = '=';
  }

  return result;
}

// Base64解码函数（辅助函数）
std::optional<std::string> base64_decode(const std::string &encoded_string) {
  static const std::string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static constexpr uint8_t base64_table[] = {
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62,  255,
      255, 255, 63,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  255, 255,
      255, 0,   255, 255, 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
      10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
      25,  255, 255, 255, 255, 255, 255, 26,  27,  28,  29,  30,  31,  32,  33,
      34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
      49,  50,  51,  255, 255, 255, 255, 255};

  std::string decoded;
  decoded.reserve(encoded_string.size() / 4 * 3);

  int bits_collected = 0;
  int bits_remaining = 0;
  uint32_t value = 0;

  for (unsigned char c : encoded_string) {
    if (c == '=') {
      break;
    }

    if (c > 127 || base64_table[c] == 255) {
      return std::nullopt;
    }

    value = (value << 6) | base64_table[c];
    bits_collected += 6;
    bits_remaining += 6;

    if (bits_remaining >= 8) {
      bits_remaining -= 8;
      decoded.push_back((value >> bits_remaining) & 0xFF);
    }
  }

  return decoded;
}

// Token校验结果结构体
enum class TokenValidationResult {
  Valid,
  InvalidFormat,
  InvalidBase64,
  Expired
};

// Token信息结构体
struct TokenInfo {
  uint64_t user_id;
  std::string username;
  std::string email;
  uint64_t timestamp;
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

// Token校验函数
std::pair<TokenValidationResult, std::optional<TokenInfo>> validate_jwt_token(
    const std::string &token) {
  // 检查令牌是否在黑名单中
  if (token_blacklist::instance().contains(token)) {
    return {TokenValidationResult::Expired,
            std::nullopt};  // 使用Expired状态表示已注销
  }

  // Base64解码
  auto decoded_opt = base64_decode(token);
  if (!decoded_opt) {
    return {TokenValidationResult::InvalidBase64, std::nullopt};
  }

  std::string decoded = *decoded_opt;

  // 解析token内容
  std::istringstream iss(decoded);
  std::string user_id_str, username, email, timestamp_str;

  // 使用冒号分隔符解析
  if (!std::getline(iss, user_id_str, ':') ||
      !std::getline(iss, username, ':') || !std::getline(iss, email, ':') ||
      !std::getline(iss, timestamp_str, ':')) {
    return {TokenValidationResult::InvalidFormat, std::nullopt};
  }

  // 转换数据类型
  try {
    uint64_t user_id = std::stoull(user_id_str);
    uint64_t timestamp = std::stoull(timestamp_str);

    // 验证token是否过期（可选，这里设置为24小时有效期）
    const uint64_t expiration_time = 24 * 60 * 60 * 1000;  // 24小时（毫秒）
    uint64_t current_time = get_jwt_timestamp_milliseconds();

    if (current_time - timestamp > expiration_time) {
      return {TokenValidationResult::Expired, std::nullopt};
    }

    // 构造TokenInfo
    TokenInfo info;
    info.user_id = user_id;
    info.username = username;
    info.email = email;
    info.timestamp = timestamp;

    return {TokenValidationResult::Valid, info};
  } catch (const std::exception &) {
    return {TokenValidationResult::InvalidFormat, std::nullopt};
  }
}

}  // namespace purecpp