#pragma once
#include <any>
#include <cinatra.hpp>
#include <iguana/json_reader.hpp>
#include <iguana/json_writer.hpp>
#include <string_view>
#include <system_error>

using namespace cinatra;
using namespace iguana;

namespace purecpp {
inline const std::vector<std::string_view> cpp_questions{
    "C++中声明指向int的常量指针, 语法是____ int* "
    "p。(请把空白部分的代码补充完整)",
    "sizeof(uint64_t)的返回值是?",
    "请输入C++中共享的智能指针。std::____ (请把空白部分的代码补充完整)",
    "请输入C++中独占的智能指针。std::____ (请把空白部分的代码补充完整)",
    "auto foo(){return new int(1);} void "
    "call_foo(){foo();} 这个call_foo函数有什么问题? ",
    "std::string str; str.reserve(100); 这个str的长度是多少?"};

inline const std::vector<std::string_view> cpp_answers{
    "const", "8", "shared_ptr", "unique_ptr", "内存泄漏", "0"};

struct register_info {
  std::string_view username;
  std::string_view email;
  std::string_view password;
  std::string_view cpp_answer;
  size_t question_index;
};

struct user_resp_data {
  uint64_t user_id;
  std::string_view username;
  std::string_view email;
  bool verification_required;
};

inline std::string make_error(std::string_view err_msg) {
  rest_response<std::string_view> data{false, std::string(err_msg)};
  std::string json;
  iguana::to_json(data, json);
  return json;
}

struct check_register_input {
  bool before(coro_http_request &req, coro_http_response &res) {
    auto body = req.get_body();
    if (body.empty()) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error("register info is empty"));
      return false;
    }

    register_info info{};
    std::error_code ec;
    iguana::from_json(info, body, ec);
    if (ec) {
      res.set_status_and_content(
          status_type::bad_request,
          make_error("register info is not a required json"));
      return false;
    }

    req.set_user_data(info);
    return true;
  }
};

struct check_cpp_answer {
  bool before(coro_http_request &req, coro_http_response &res) {
    register_info info = std::any_cast<register_info>(req.get_user_data());
    bool r = cpp_answers[info.question_index] == info.cpp_answer;

    if (!r) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error("问题的答案不对。"));
      return false;
    }
    return true;
  }
};

struct check_user_name {
  bool before(coro_http_request &req, coro_http_response &res) {
    register_info info = std::any_cast<register_info>(req.get_user_data());
    if (info.username.empty() || info.username.size() > 20) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error("用户名长度非法应改为1-20。"));
      return false;
    }

    const std::regex username_regex("^[a-zA-Z0-9_-]+$");

    bool r = std::regex_match(std::string(info.username), username_regex);
    if (!r) {
      res.set_status_and_content(
          status_type::bad_request,
          make_error(
              "只允许字母 (a-z, A-Z), 数字 (0-9), 下划线 (_), 连字符 (-)。"));
      return false;
    }
    return true;
  }
};

struct check_email {
  bool before(coro_http_request &req, coro_http_response &res) {
    register_info info = std::any_cast<register_info>(req.get_user_data());
    if (info.email.empty() || info.email.size() > 254) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error("邮箱格式不合法。"));
      return false;
    }

    const std::regex email_regex(
        R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    bool r = std::regex_match(std::string{info.email}, email_regex);

    if (!r) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error("邮箱格式不合法。"));
      return false;
    }
    return true;
  }
};

struct check_password {
  bool before(coro_http_request &req, coro_http_response &res) {
    register_info info = std::any_cast<register_info>(req.get_user_data());
    if (info.password.size() < 6 || info.password.size() > 20) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error("密码长度不合法，长度6-20位。"));
      return false;
    }

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    // 2. 字符类型要求 (至少包含大小写字母、数字和特殊字符各一个)
    for (char c : info.password) {
      if (std::isupper(static_cast<unsigned char>(c))) {
        has_upper = true;
      } else if (std::islower(static_cast<unsigned char>(c))) {
        has_lower = true;
      } else if (std::isdigit(static_cast<unsigned char>(c))) {
        has_digit = true;
      }
    }
    bool r = has_upper && has_lower && has_digit;
    if (!r) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error("密码至少包含大小写字母和数字。"));
      return false;
    }
    return true;
  }
};

} // namespace purecpp