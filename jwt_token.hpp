#pragma once
#include "cinatra/utils.hpp"
#include "config.hpp"
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
  return cinatra::base64_encode(token);
}

// Token校验结果结构体
enum class TokenValidationResult : int32_t {
  Valid,
  InvalidFormat,
  InvalidBase64,
  Expired
};

// Token信息结构体
struct token_info {
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
std::pair<TokenValidationResult, std::optional<token_info>>
validate_jwt_token(const std::string &token) {
  // 检查令牌是否在黑名单中
  if (token_blacklist::instance().contains(token)) {
    return {TokenValidationResult::Expired,
            std::nullopt}; // 使用Expired状态表示已注销
  }

  // Base64解码
  auto decoded_opt = cinatra::base64_decode(token);
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
    const uint64_t expiration_time =
        purecpp_config::get_instance().user_cfg_.token_expiration_minutes * 60 *
        1000; // 24小时（毫秒）
    uint64_t current_time = get_jwt_timestamp_milliseconds();

    if (current_time - timestamp > expiration_time) {
      return {TokenValidationResult::Expired, std::nullopt};
    }

    // 构造token_info
    token_info info;
    info.user_id = user_id;
    info.username = username;
    info.email = email;
    info.timestamp = timestamp;

    return {TokenValidationResult::Valid, info};
  } catch (const std::exception &) {
    return {TokenValidationResult::InvalidFormat, std::nullopt};
  }
}

} // namespace purecpp