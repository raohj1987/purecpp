#pragma once

#include <cinatra.hpp>
#include <fstream>
#include <iguana/json_reader.hpp>
#include <optional>
#include <string>
#include <vector>

namespace purecpp {

// 单个路由限流规则配置
struct rate_limit_rule {
  std::string path;    // 路由路径或正则表达式
  int max_requests;    // 最大请求次数
  int window_seconds;  // 时间窗口（秒）
  bool enabled = true; // 是否启用
};

/**
 * @brief 用户配置结构体
 */
struct user_config {
  // 安全设置
  int32_t lock_failed_attempts;     // 登录失败锁定阈值
  int32_t lock_duration_minutes;    // 账号锁定持续时间（分钟）
  int32_t access_token_exp_minutes; // JWT令牌过期时间（分钟）
  int32_t refresh_token_exp_days;   // Refresh Token过期时间（天）
  std::string access_token_secret;  // JWT令牌密钥
  std::string refresh_token_secret; // Refresh Token密钥

  // 邮件服务器配置
  std::string smtp_host;          // SMTP服务器主机名
  int smtp_port;                  // SMTP服务器端口
  std::string smtp_user;          // SMTP服务器用户名
  std::string smtp_password;      // SMTP服务器密码
  std::string smtp_from_email;    // 发件人邮箱地址
  std::string smtp_from_name;     // 发件人名称
  std::string web_server_url;     // 网页服务器URL
  std::string default_avatar_url; // 默认头像URL

  // 基于路由的限流配置
  std::vector<rate_limit_rule> rate_limit_rules; // 限流规则列表
}; // 用户配置结构体，包含安全设置和邮件服务器配置

/**
 * @brief 配置类，用于存储全局配置
 */
class purecpp_config {
public:
  purecpp_config(const purecpp_config &) = delete;
  purecpp_config(purecpp_config &&) = delete;
  purecpp_config &operator=(const purecpp_config &) = delete;
  purecpp_config &operator=(purecpp_config &&) = delete;

  static purecpp_config &get_instance() {
    // C++11起静态局部变量就是线程安全的，C++17保持该特性
    static purecpp_config instance;
    return instance;
  }

  /**
   * @brief 从JSON文件加载配置
   * @param filename JSON文件名
   */
  void load_config(const std::string &filename) {
    // 从JSON文件加载配置
    std::ifstream file(filename, std::ios::in);
    if (!file.is_open()) {
      CINATRA_LOG_ERROR << "no config file";
      return;
    }

    std::string json;
    json.resize(4096);
    file.read(json.data(), json.size());
    iguana::from_json(user_cfg_, json);
  }

private:
  // ======== 私有构造/析构：保证单例 ========
  purecpp_config() = default;
  ~purecpp_config() = default;

public:
  user_config user_cfg_;
};
} // namespace purecpp
