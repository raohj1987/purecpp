#pragma once
#include <string>

#include <ormpp/connection_pool.hpp>
#include <ormpp/dbng.hpp>
#include <ormpp/mysql.hpp>
using namespace ormpp;

namespace purecpp {
// 用户等级枚举
enum class UserLevel : int32_t {
  LEVEL_1 = 1,  // 等级1 - 新手
  LEVEL_2 = 2,  // 等级2 - 入门
  LEVEL_3 = 3,  // 等级3 - 进阶
  LEVEL_4 = 4,  // 等级4 - 熟练
  LEVEL_5 = 5,  // 等级5 - 专家
  LEVEL_6 = 6,  // 等级6 - 大师
  LEVEL_7 = 7,  // 等级7 - 宗师
  LEVEL_8 = 8,  // 等级8 - 传奇
  LEVEL_9 = 9,  // 等级9 - 神话
  LEVEL_10 = 10 // 等级10 - 不朽
};

// 用户头衔枚举
enum class UserTitle : int32_t {
  NEWBIE = 0,           // 新手
  DEVELOPER = 1,        // 开发者
  SENIOR_DEVELOPER = 2, // 高级开发者
  ENGINEER = 3,         // 工程师
  SENIOR_ENGINEER = 4,  // 高级工程师
  ARCHITECT = 5,        // 架构师
  TECH_LEAD = 6,        // 技术负责人
  EXPERT = 7,           // 专家
  MASTER = 8,           // 大师
  LEGEND = 9            // 传奇
};

enum EmailVerifyStatus : int32_t {
  UNVERIFIED = 0, // 未验证
  VERIFIED = 1,   // 已验证
};

// 在线状态
inline constexpr std::string_view STATUS_OF_OFFLINE = "Offline";
inline constexpr std::string_view STATUS_OF_ONLINE = "Online";
inline constexpr std::string_view STATUS_OF_AWAY = "Away";

// database config
struct db_config {
  std::string db_ip;
  int db_port;
  std::string db_name;
  std::string db_user_name;
  std::string db_pwd;

  int db_conn_num;
  int db_conn_timeout; // seconds
};

struct users_t {
  uint64_t id;
  std::array<char, 21> user_name; // unique, not null
  std::array<char, 254> email;    // unique, not null
  std::string_view pwd_hash;      // not null
  std::string status;             // 在线状态Online, Offline, Away
  EmailVerifyStatus is_verifyed;  // 邮箱是否已验证(0:未验证, 1:已验证)
  uint64_t created_at;
  uint64_t last_active_at; // 最后活跃时间

  // 用户身份信息
  UserTitle title;     // 头衔枚举
  std::string role;    // 角色，如"user"、"admin"、"moderator"
  uint64_t experience; // 经验值
  UserLevel level;     // 用户等级枚举

  // 个人资料信息
  std::optional<std::string> bio;    // 个人简介
  std::optional<std::string> avatar; // 头像URL

  // 登录安全相关字段
  uint32_t login_attempts;    // 登录失败次数
  uint64_t last_failed_login; // 最后一次登录失败时间戳
};
// 注册users_t的主键
REGISTER_AUTO_KEY(users_t, id);

inline constexpr std::string_view get_alias_struct_name(users_t *) {
  return "users"; // 表名默认结构体名字(users_t), 这里可以修改表名
}

// 用户token表
enum class TokenType : int32_t {
  RESET_PASSWORD = 0, // 重置密码
  VERIFY_EMAIL = 1,   // 验证邮箱
  REFRESH_TOKEN = 2,  // 刷新token
};

struct users_token_t {
  uint64_t id;
  uint64_t user_id;
  TokenType token_type; // 0: 重置密码, 1: 验证邮箱
  std::array<char, 129> token;
  uint64_t created_at;
  uint64_t expires_at;
};
// 注册users_token_t的主键
REGISTER_AUTO_KEY(users_token_t, id);

inline constexpr std::string_view get_alias_struct_name(users_token_t *) {
  return "user_tokens"; // 表名
}

// 文章相关的表
struct articles_t {
  uint64_t article_id = 0;
  int tag_id; // 外键
  std::string title;
  std::string abstraction; // 摘要
  std::string content;
  std::array<char, 8> slug; // 随机数，用于生成url的后缀
  uint64_t author_id;       // 外键
  uint64_t created_at;
  uint64_t updated_at;
  uint32_t views_count;
  uint32_t comments_count;
  uint64_t reviewer_id;       // 审核人id 外键
  std::string review_comment; // 审核意见
  int featured_weight;        // 置顶，精华
  uint64_t review_date;       // 审核完成时间
  std::string review_status =
      "pending_review"; // pending_review (待审核), rejected (已拒绝), accepted
  std::string status;   // 状态：published, draft, archived
  bool is_deleted;
};
inline constexpr std::string_view get_alias_struct_name(articles_t *) {
  return "articles";
}

struct article_comments_t {
  uint64_t comment_id = 0;
  uint64_t article_id; // 外键
  uint64_t user_id;    // 外键
  std::string content;
  uint64_t parent_comment_id; // 指向父评论
  uint64_t parent_user_id;
  std::array<char, 21> parent_user_name; // unique, not null
  std::array<char, 16> ip;               // 评论者IP地址
  uint64_t created_at;
  uint64_t updated_at;
};
REGISTER_AUTO_KEY(article_comments_t, comment_id);
inline constexpr std::string_view get_alias_struct_name(article_comments_t *) {
  return "article_comments";
}

struct tags_t {
  int tag_id;
  std::array<char, 50> name;
};
inline constexpr std::string_view get_alias_struct_name(tags_t *) {
  return "tags";
}

template <typename T> struct rest_response {
  bool success = true;
  std::string message;
  std::optional<std::vector<std::string>> errors;
  std::optional<T> data;
  std::string timestamp;
  int code = 200;
};
} // namespace purecpp