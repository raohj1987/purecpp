#pragma once
#include "common.hpp"
#include "jwt_token.hpp"
#include <any>
#include <cinatra.hpp>
#include <string_view>
#include <system_error>

using namespace cinatra;
using namespace iguana;

#include "error_info.hpp"
#include "user_dto.hpp"
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
std::string cleanup_markdown(const std::string &markdown_text) {
  std::string text = markdown_text;

  // 1. 清理链接和图片 (保留链接文本)
  // 匹配 [text](url) 或 ![text](url)
  text = std::regex_replace(text, std::regex("!\\[(.*?)\\]\\(.*?\\)"),
                            "$1"); // 图片
  text = std::regex_replace(text, std::regex("\\[(.*?)\\]\\(.*?\\)"),
                            "$1"); // 链接

  // 2. 清理粗体和斜体 (**bold** or *italic*)
  // 匹配 **...** 和 *...*
  text = std::regex_replace(text, std::regex("(\\*\\*|__)(.*?)\\1"), "$2");
  text = std::regex_replace(text, std::regex("(\\*|_)(.*?)\\1"), "$2");

  // 3. 清理代码块和行内代码 (```code``` or `code`)
  text = std::regex_replace(text, std::regex("```[\\s\\S]*?```"),
                            "");                                // 移除代码块
  text = std::regex_replace(text, std::regex("`(.*?)`"), "$1"); // 行内代码

  // 4. 清理标题 (# H1, ## H2, etc.)
  text = std::regex_replace(text, std::regex("^#+\\s*"), "");

  // 5. 清理列表和引用 (> quote)
  text = std::regex_replace(text, std::regex("^[*-+]\\s"), ""); // 列表
  text = std::regex_replace(text, std::regex("^>\\s"), "");     // 引用

  // 6. 最终清理多余的换行和空格
  // 替换多个换行符为一个空格（将段落连起来）
  text = std::regex_replace(text, std::regex("\\n+"), " ");

  return text;
}
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

// 登录相关的验证结构体
struct check_login_input {
  bool before(coro_http_request &req, coro_http_response &res) {
    auto body = req.get_body();
    if (body.empty()) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error(PURECPP_ERROR_LOGIN_INFO_EMPTY));
      return false;
    }

    login_info info{};
    std::error_code ec;
    iguana::from_json(info, body, ec);
    if (ec) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error(PURECPP_ERROR_LOGIN_JSON_INVALID));
      return false;
    }
    // 校验用户名、密码不能为空
    if (info.username.empty() || info.password.empty()) {
      res.set_status_and_content(
          status_type::bad_request,
          make_error(PURECPP_ERROR_LOGIN_CREDENTIALS_EMPTY));
      return false;
    }

    req.set_user_data(info);
    return true;
  }
};

// Token验证结构体
struct check_token {
  bool before(coro_http_request &req, coro_http_response &res) {
    // 从请求头获取令牌
    std::string token;
    auto headers = req.get_headers();
    for (auto &header : headers) {
      if (cinatra::iequal0(header.name, "Authorization")) {
        // 提取Bearer令牌
        std::string_view auth_header = header.value;
        if (auth_header.size() > 7 && auth_header.substr(0, 7) == "Bearer ") {
          token = std::string(auth_header.substr(7));
          break;
        }
      }
    }

    // 检查令牌是否存在
    if (token.empty()) {
      res.set_status_and_content(status_type::unauthorized,
                                 make_error(PURECPP_ERROR_TOKEN_MISSING));
      return false;
    }

    // 验证令牌
    auto [result, info] = validate_jwt_token(token);

    if (result != TokenValidationResult::Valid) {
      // 令牌无效，返回错误信息
      std::string error_msg;
      switch (result) {
      case TokenValidationResult::InvalidFormat:
        error_msg = PURECPP_ERROR_TOKEN_INVALID;
        break;
      case TokenValidationResult::InvalidBase64:
        error_msg = PURECPP_ERROR_TOKEN_INVALID;
        break;
      case TokenValidationResult::Expired:
        error_msg = PURECPP_ERROR_TOKEN_EXPIRED;
        break;
      default:
        error_msg = PURECPP_ERROR_TOKEN_INVALID;
      }

      res.set_status_and_content(status_type::unauthorized,
                                 make_error(error_msg));
      return false;
    }

    // TODO 将token信息保存到请求中,
    // 这个方式不是很友好，后面有更好的方式传递时再优化 req.set_user_data(info);
    return true;
  }
};

// 修改密码相关的验证结构体
struct check_change_password_input {
  bool before(coro_http_request &req, coro_http_response &res) {
    auto body = req.get_body();
    if (body.empty()) {
      res.set_status_and_content(
          status_type::bad_request,
          make_error(PURECPP_ERROR_CHANGE_PASSWORD_EMPTY));
      return false;
    }

    change_password_info info{};
    std::error_code ec;
    iguana::from_json(info, body, ec);
    if (ec) {
      res.set_status_and_content(
          status_type::bad_request,
          make_error(PURECPP_ERROR_CHANGE_PASSWORD_JSON_INVALID));
      return false;
    }

    // 校验用户ID、旧密码、新密码不能为空
    if (info.user_id == 0 || info.old_password.empty() ||
        info.new_password.empty()) {
      res.set_status_and_content(
          status_type::bad_request,
          make_error(PURECPP_ERROR_CHANGE_PASSWORD_REQUIRED_FIELDS));
      return false;
    }

    req.set_user_data(info);
    return true;
  }
};

struct check_new_password {
  bool before(coro_http_request &req, coro_http_response &res) {
    change_password_info info =
        std::any_cast<change_password_info>(req.get_user_data());

    // 检查新密码长度
    if (info.new_password.size() < 6 || info.new_password.size() > 20) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error(PURECPP_ERROR_PASSWORD_LENGTH));
      return false;
    }

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    // 检查新密码字符类型要求
    for (char c : info.new_password) {
      if (std::isupper(static_cast<unsigned char>(c))) {
        has_upper = true;
      } else if (std::islower(static_cast<unsigned char>(c))) {
        has_lower = true;
      } else if (std::isdigit(static_cast<unsigned char>(c))) {
        has_digit = true;
      }
    }

    // 至少包含大小写字母和数字
    bool valid = has_upper && has_lower && has_digit;
    if (!valid) {
      res.set_status_and_content(status_type::bad_request,
                                 make_error(PURECPP_ERROR_PASSWORD_COMPLEXITY));
      return false;
    }

    // 新密码不能与旧密码相同
    if (info.new_password == info.old_password) {
      res.set_status_and_content(
          status_type::bad_request,
          make_error(PURECPP_ERROR_PASSWORD_NEW_SAME_AS_OLD));
      return false;
    }

    return true;
  }
};
// 日志切面工具
struct log_request_response {
  // 在请求处理前记录请求信息
  bool before(coro_http_request &req, coro_http_response &res) {
    // 记录请求信息
    std::ostringstream log_stream;

    // 格式化时间戳
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch() % std::chrono::seconds(1));

    std::tm now_tm = *std::localtime(&now_c);

    log_stream << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << "."
               << std::setfill('0') << std::setw(3) << now_ms.count() << "] "
               << "[REQUEST] " << req.get_method() << " " << req.full_url()
               << " " << std::endl;

    // 记录请求体
    auto body = req.get_body();
    if (!body.empty()) {
      log_stream << "[REQUEST BODY]: "
                 << (body.size() > 1000
                         ? std::string(body.substr(0, 1000)) + "..."
                         : std::string(body))
                 << std::endl;
    }

    // 输出日志
    std::cout << log_stream.str() << std::flush;

    return true; // 继续处理请求
  }

  // 在请求处理后记录响应信息
  bool after(coro_http_request &req, coro_http_response &res) {
    // 记录响应信息
    std::ostringstream log_stream;

    // 格式化时间戳
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch() % std::chrono::seconds(1));

    std::tm now_tm = *std::localtime(&now_c);

    log_stream << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << "."
               << std::setfill('0') << std::setw(3) << now_ms.count() << "] "
               << "[RESPONSE] " << req.get_method() << " " << req.full_url()
               << " " << "Status: " << static_cast<int>(res.status())
               << std::endl;

    // 记录响应体（只记录前1000个字符以避免日志过长）
    auto body = res.content();
    if (!body.empty()) {
      log_stream << "[RESPONSE BODY]: "
                 << (body.size() > 1000
                         ? std::string(body.substr(0, 1000)) + "..."
                         : std::string(body))
                 << std::endl;
    }

    log_stream << "----------------------------------------" << std::endl;

    // 输出日志
    std::cout << log_stream.str() << std::flush;

    return true; // 继续处理后续操作
  }
};
} // namespace purecpp