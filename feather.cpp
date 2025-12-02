#include <cinatra.hpp>

#include <filesystem>
#include <iguana/json_reader.hpp>
#include <iguana/json_writer.hpp>
#include <iguana/prettify.hpp>
#include <iostream>

#include "entity.hpp"

using namespace cinatra;
using namespace ormpp;
using namespace purecpp;

struct test_optional {
  int id;
  std::optional<std::string> name;
  std::optional<int> age;
};

/*
// 成功响应示例
{
    "success": true,
    "message": "注册成功",
    "data": {
        "user_id": 12345,
        "username": "testuser",
        "email": "test@example.com",
        "verification_required": true
    },
    "timestamp": "2024-01-15T10:30:00Z",
    "code": 200
}

// 失败响应示例
{
    "success": false,
    "message": "用户名已存在",
    "errors": {
        "username": "用户名必须大于4个字符。",
        "email": "该邮箱已存在。",
        "cpp_answer": "答案错误，请重新计算。"
    },
    "timestamp": "2024-01-15T10:30:00Z",
    "code": 400
}
*/

struct rest_data {
  uint64_t user_id;
  std::string username;
  std::string email;
  bool verification_required;
};
struct rest_response {
  bool success;
  std::string message;
  std::optional<std::vector<std::string>> errors;
  std::optional<rest_data> data;
  std::string timestamp;
  int code;
};

// database
void init_db() {
  std::ifstream file("cfg/db_config.json", std::ios::in);
  if (!file.is_open()) {
    std::cout << "no config file\n";
    return;
  }

  std::string json;
  json.resize(1024);
  file.read(json.data(), json.size());
  db_config conf;
  iguana::from_json(conf, json);

  auto &pool = connection_pool<dbng<mysql>>::instance();
  try {
    pool.init(conf.db_conn_num, conf.db_ip, conf.db_user_name, conf.db_pwd,
              conf.db_name.data(), conf.db_conn_timeout, conf.db_port);
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
    return;
  }

  auto conn = pool.get();
  bool r = conn->create_datatable<users_t>(
      ormpp_unique{{"user_name", "email", "pwd_hash"}},
      ormpp_not_null{{"user_name", "email", "pwd_hash"}});

  auto vec = conn->query_s<users_t>();
  std::cout << vec.size() << "\n";
}

int main() {
  init_db();

  coro_http_server server(std::thread::hardware_concurrency(), 3389);
  server.set_file_resp_format_type(file_resp_format_type::chunked);
  server.set_static_res_dir("", "html");
  server.set_http_handler<GET, POST>(
      "/", [](coro_http_request &req, coro_http_response &resp) {
        resp.set_status_and_content(status_type::ok, "hello purecpp");
      });
  server.set_http_handler<GET, POST>(
      "/api/v1/register", [](coro_http_request &req, coro_http_response &resp) {
        rest_response data{};
        // data.success = true;
        // data.message = "注册成功";
        // data.data = rest_data{1, "tom", "example@163.com", true};
        data.success = false;
        data.message = "email invalid";
        data.errors = {{"emal invlaid"}};

        std::string json;
        iguana::to_json(data, json);
        resp.set_status_and_content(status_type::bad_request, std::move(json));
      });
  server.sync_start();
}