#pragma once
#include <cinttypes>
#include <string>

#include <ormpp/connection_pool.hpp>
#include <ormpp/dbng.hpp>
#include <ormpp/mysql.hpp>
using namespace ormpp;

namespace purecpp {
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
  int is_verifyed;                // 邮箱是否已验证
  uint64_t created_at;
  uint64_t last_active_at; // 最后活跃时间
};

inline constexpr std::string_view get_alias_struct_name(users_t *) {
  return "users"; // 表名默认结构体名字(users_t), 这里可以修改表名
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