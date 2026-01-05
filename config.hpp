#pragma once

#include <fstream> // 鐢ㄤ簬 std::ifstream
#include <iguana/json_reader.hpp>
#include <iostream> // 鐢ㄤ簬 std::cout
#include <string>

namespace purecpp {
/**
 * @brief 用户配置结构体
 */
struct user_config {
  int32_t lock_failed_attempts;     // 登录失败锁定阈值
  int32_t lock_duration_minutes;    // 账号锁定持续时间（分钟）
  int32_t token_expiration_minutes; // JWT令牌过期时间（分钟）
  std::string smtp_host;            // SMTP服务器主机名
  int smtp_port;                    // SMTP服务器端口
  std::string smtp_user;            // SMTP服务器用户名
  std::string smtp_password;        // SMTP服务器密码
  std::string smtp_from_email;      // 发件人邮箱地址
  std::string smtp_from_name;       // 发件人名称
  std::string reset_password_url;   // 密码重置URL
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
    json.resize(1024);
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
