#include <cinatra.hpp>

#include <algorithm>
#include <filesystem>
#include <iguana/json_reader.hpp>
#include <iguana/json_writer.hpp>
#include <random>
#include <vector>

#include "articles.hpp"
#include "entity.hpp"
#include "rate_limiter.hpp"
#include "tags.hpp"
#include "user_aspects.hpp"
#include "user_login.hpp"
#include "user_password.hpp"
#include "user_register.hpp"

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

// database
bool init_db() {
  std::ifstream file("cfg/db_config.json", std::ios::in);
  if (!file.is_open()) {
    CINATRA_LOG_ERROR << "no config file";
    return false;
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
    CINATRA_LOG_ERROR << e.what();
    return false;
  }

  auto conn = pool.get();
  conn->create_datatable<users_t>(
      ormpp_auto_key{"id"}, ormpp_unique{{"user_name"}},
      ormpp_unique{{"email"}},
      ormpp_not_null{{"user_name", "email", "pwd_hash"}});

  conn->create_datatable<tags_t>(ormpp_auto_key{"tag_id"},
                                 ormpp_unique{{"name"}});
  // std::vector<tags_t> tags{{0, "开源项目"}, {0, "社区活动"}, {0, "元编程"},
  //                          {0, "代码精粹"}, {0, "技术探讨"}, {0, "语言特性"},
  //                          {0, "程序人生"}, {0, "并发编程"}, {0,
  //                          "开发心得"}};
  // conn->insert(tags);
  conn->create_datatable<article_comments>(ormpp_auto_key{"comment_id"});
  conn->create_datatable<articles_t>(ormpp_auto_key{"article_id"},
                                     ormpp_unique{{"slug"}});

  // 创建密码重置token表
  bool created = conn->create_datatable<users_token_t>(
      ormpp_auto_key{"id"}, ormpp_unique{{"user_id", "token_type"}},
      ormpp_unique{{"token"}},
      ormpp_not_null{
          {"user_id", "token_type", "token", "created_at", "expires_at"}});
  if (created) {
    CINATRA_LOG_INFO << "Table 'users_token' created successfully.";
  } else {
    CINATRA_LOG_ERROR << "Table 'users_token' create error.";
  }
  return true;
}

size_t get_question_index() {
  static unsigned seed =
      std::chrono::system_clock::now().time_since_epoch().count();
  static std::mt19937 generator(seed);

  std::uniform_int_distribution<size_t> distribution(0,
                                                     cpp_questions.size() - 1);
  size_t random_index = distribution(generator);
  return random_index;
}

struct question_resp {
  size_t index;
  std::string_view question;
};

int main() {
  if (!init_db()) {
    return -1;
  }
  // 从配置文件加载配置
  purecpp_config::get_instance().load_config("cfg/user_config.json");

  // 初始化限流器
  rate_limiter::instance().init_from_config();

  auto &db_pool = connection_pool<dbng<mysql>>::instance();

  coro_http_server server(std::thread::hardware_concurrency(), 3389);
  server.set_file_resp_format_type(file_resp_format_type::chunked);
  server.set_static_res_dir("", "html");
  server.set_http_handler<GET, POST>(
      "/", [](coro_http_request &req, coro_http_response &resp) {
        resp.set_status_and_content(status_type::ok, "hello purecpp");
      });

  server.set_http_handler<GET>(
      "/api/v1/get_questions",
      [](coro_http_request &req, coro_http_response &resp) {
        size_t random_index = get_question_index();
        question_resp question{random_index, cpp_questions[random_index]};
        rest_response<question_resp> data{};
        data.data = question;

        std::string json;
        iguana::to_json(data, json);
        resp.set_content_type<resp_content_type::json>();
        resp.set_status_and_content(status_type::ok, std::move(json));
      });

  user_register_t usr_reg{};
  server.set_http_handler<POST>(
      "/api/v1/register", &user_register_t::handle_register, usr_reg,
      check_register_input{}, check_cpp_answer{}, check_user_name{},
      check_email{}, check_password{}, rate_limiter_aspect{});

  // 邮箱验证相关路由
  server.set_http_handler<POST>(
      "/api/v1/verify_email", &user_register_t::handle_verify_email, usr_reg,
      log_request_response{}, check_verify_email_input{});

  server.set_http_handler<POST>("/api/v1/resend_verify_email",
                                &user_register_t::handle_resend_verify_email,
                                usr_reg, log_request_response{});

  user_login_t usr_login{};
  server.set_http_handler<POST>("/api/v1/login", &user_login_t::handle_login,
                                usr_login, log_request_response{},
                                check_login_input{});

  // 添加退出登录路由
  server.set_http_handler<POST, GET>(
      "/api/v1/logout", &user_login_t::handle_logout, usr_login,
      log_request_response{}, check_token{}, check_logout_input{});

  user_password_t usr_password{};
  server.set_http_handler<POST>(
      "/api/v1/change_password", &user_password_t::handle_change_password,
      usr_password, log_request_response{}, check_token{},
      check_change_password_input{}, check_new_password{});

  // 添加忘记密码和重置密码的路由
  server.set_http_handler<POST>(
      "/api/v1/forgot_password", &user_password_t::handle_forgot_password,
      usr_password, log_request_response{}, check_forgot_password_input{});

  server.set_http_handler<POST>(
      "/api/v1/reset_password", &user_password_t::handle_reset_password,
      usr_password, log_request_response{}, check_reset_password_input{},
      check_reset_password{});
  tags tag{};
  server.set_http_handler<GET>("/api/v1/get_tags", &tags::get_tags, tag);

  articles article{};
  server.set_http_handler<POST>("/api/v1/new_article",
                                &articles::handle_new_article, article);
  server.set_http_handler<GET>("/api/v1/get_articles", &articles::get_articles,
                               article);
  server.set_http_handler<GET>("/api/v1/get_article_count",
                               &articles::get_article_count, article);
  server.set_http_handler<GET>("/api/v1/article/:slug", &articles::show_article,
                               article);
  server.set_http_handler<POST>("/api/v1/edit_article", &articles::edit_article,
                                article, check_edit_article{});
  server.set_http_handler<GET>("/api/v1/get_pending_articles",
                               &articles::get_pending_articles, article);
  server.set_http_handler<POST>("/api/v1/review_pending_article",
                                &articles::handle_review_article, article);
  server.sync_start();
}
