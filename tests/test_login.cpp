#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

// 包含必要的头文件
#include <cinatra/coro_http_client.hpp>
#include <iguana/json_reader.hpp>
#include <iguana/json_writer.hpp>

#include "../entity.hpp"
#include "../user_aspects.hpp"
#include "../user_dto.hpp"
#include "../user_login.hpp"
#include "unicode_utils.hpp"

using namespace purecpp;
using namespace cinatra;
using namespace iguana;

// 全局测试数据
std::string g_username;
std::string g_email;
std::string g_password = "Password123";
std::string register_url = "http://127.0.0.1:3389/api/v1/register";
std::string login_url = "http://127.0.0.1:3389/api/v1/login";
std::string forget_password_url = "http://127.0.0.1:3389/api/v1/forgot_password";
std::string reset_password_url = "http://127.0.0.1:3389/api/v1/reset_password";

// 注册测试用户
void register_test_user() {
  coro_http_client client{};
  register_info info;

  // 生成唯一的用户名和邮箱
  g_username = "testuser_" + std::to_string(get_timestamp_milliseconds());
  g_email = g_username + "@example.com";

  info.username = g_username;
  info.email = g_email;
  info.password = g_password;
  info.cpp_answer = "const";  // 第一个问题的正确答案
  info.question_index = 0;

  string_stream ss;
  to_json(info, ss);

  auto resp = client.post(register_url, ss.c_str(), req_content_type::json);
  std::cout << "Register response: " << resp.resp_body << std::endl;

  // 验证注册成功
  CHECK(resp.resp_body.find("\"success\":true") != std::string::npos);
}

// 测试用户登录功能
TEST_CASE("User Login API Tests") {
  // 在所有测试之前注册一个测试用户
  register_test_user();

  SUBCASE("Login with correct username and password") {
    coro_http_client client{};
    login_info info;

    info.username = g_username;
    info.password = g_password;

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with username response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含成功标志
    CHECK(resp.resp_body.find("\"success\":true") != std::string::npos);
    // 验证响应中包含token
    CHECK(resp.resp_body.find("\"token\"") != std::string::npos);
    // 验证响应中包含用户信息
    CHECK(resp.resp_body.find("\"username\"") != std::string::npos);
    CHECK(resp.resp_body.find("\"email\"") != std::string::npos);
  }

  SUBCASE("Login with correct email and password") {
    coro_http_client client{};
    login_info info;

    info.username = g_email;  // 使用邮箱作为用户名
    info.password = g_password;

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with email response: " << resp.resp_body << std::endl;

    // 验证响应中包含成功标志
    CHECK(resp.resp_body.find("\"success\":true") != std::string::npos);
    // 验证响应中包含token
    CHECK(resp.resp_body.find("\"token\"") != std::string::npos);
  }

  SUBCASE("Login with incorrect password") {
    coro_http_client client{};
    login_info info;

    info.username = g_username;
    info.password = "wrongPassword123";

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with wrong password response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含失败标志
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证错误信息 - 使用unicode_to_utf8转换后再进行判断
    std::string expected_error = "用户名或密码错误";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_error) != std::string::npos);
  }

  SUBCASE("Login with non-existent username") {
    coro_http_client client{};
    login_info info;

    info.username = "nonexistent_user_123456";
    info.password = g_password;

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with non-existent username response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含失败标志
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证错误信息 - 使用unicode_to_utf8转换后再进行判断
    std::string expected_error = "用户名或密码错误";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_error) != std::string::npos);
  }

  SUBCASE("Login with non-existent email") {
    coro_http_client client{};
    login_info info;

    info.username = "nonexistent_email_123456@example.com";
    info.password = g_password;

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with non-existent email response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含失败标志
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证错误信息 - 使用unicode_to_utf8转换后再进行判断
    std::string expected_error = "用户名或密码错误";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_error) != std::string::npos);
  }

  SUBCASE("Login with empty request body") {
    coro_http_client client{};

    // 发送空的请求体
    auto resp = client.post(login_url, "", req_content_type::json);
    std::cout << "Login with empty body response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含失败标志
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证错误信息
    CHECK(resp.resp_body.find("login info is empty") != std::string::npos);
  }

  SUBCASE("Login with invalid JSON format") {
    coro_http_client client{};

    // 发送格式错误的JSON
    std::string invalid_json =
        "{\"username\":\"testuser\",\"password\":\"password123\"";
    auto resp = client.post(login_url, invalid_json, req_content_type::json);
    std::cout << "Login with invalid JSON response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含失败标志
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证错误信息
    CHECK(resp.resp_body.find("login info is not a required json") !=
          std::string::npos);
  }
}

// 主测试用例
TEST_CASE("User Login API Comprehensive Tests") {
  // 首先注册测试用户
  register_test_user();

  // 运行所有登录相关测试
  SUBCASE("Login with correct username and password") {
    coro_http_client client{};
    login_info info;

    info.username = g_username;
    info.password = g_password;

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with username response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含成功标志
    CHECK(resp.resp_body.find("\"success\":true") != std::string::npos);
    CHECK(resp.resp_body.find("\"username\":\"" + g_username + "\"") !=
          std::string::npos);
    CHECK(resp.resp_body.find("\"email\":\"" + g_email + "\"") !=
          std::string::npos);
    CHECK(resp.resp_body.find("\"token\":") != std::string::npos);
  }

  SUBCASE("Login with correct email and password") {
    coro_http_client client{};
    login_info info;

    info.username = g_email;
    info.password = g_password;

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with email response: " << resp.resp_body << std::endl;

    // 验证响应中包含成功标志
    CHECK(resp.resp_body.find("\"success\":true") != std::string::npos);
    // 验证响应中包含token
    CHECK(resp.resp_body.find("\"token\"") != std::string::npos);
  }

  SUBCASE("Login with incorrect password") {
    coro_http_client client{};
    login_info info;

    info.username = g_username;
    info.password = "wrongPassword123";

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with wrong password response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含失败标志
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证错误信息 - 使用unicode_to_utf8转换后再进行判断
    std::string expected_error = "用户名或密码错误";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_error) != std::string::npos);
  }

  SUBCASE("Login with non-existent username") {
    coro_http_client client{};
    login_info info;

    info.username = "nonexistent_user_123456";
    info.password = g_password;

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(login_url, ss.c_str(), req_content_type::json);
    std::cout << "Login with non-existent username response: " << resp.resp_body
              << std::endl;

    // 验证响应中包含失败标志
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证错误信息 - 使用unicode_to_utf8转换后再进行判断
    std::string expected_error = "用户名或密码错误";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_error) != std::string::npos);
  }
}

// 测试修改密码功能
TEST_CASE("User Change Password Tests") {
  // 首先注册测试用户并登录获取token
  register_test_user();
  
  // 登录获取用户信息和token
  uint64_t user_id = 0;
  std::string token;
  
  {
    coro_http_client client{};
    std::string url{"http://127.0.0.1:3389/api/v1/login"};
    login_info info;

    info.username = g_username;
    info.password = g_password;

    string_stream ss;
    to_json(info, ss);

    auto resp = client.post(url, ss.c_str(), req_content_type::json);
    std::cout << "Login response (for change password test): " << resp.resp_body
              << std::endl;

    // 验证响应
    CHECK(resp.resp_body.find("\"success\":true") != std::string::npos);
    
    if (resp.resp_body.find("\"token\":\"") != std::string::npos) {
      // 解析响应获取user_id和token
      rest_response<login_resp_data> login_data {};
      iguana::from_json(login_data, resp.resp_body);
      user_id = login_data.data->user_id;
      token = login_data.data->token;
    }
  }
  
  // 测试修改密码功能
  SUBCASE("Change password successfully") {
    coro_http_client client{};
    std::string url{"http://127.0.0.1:3389/api/v1/change_password"};
    change_password_info info;
    
    info.user_id = user_id;
    info.old_password = g_password;
    info.new_password = "NewPassword123";
    
    string_stream ss;
    to_json(info, ss);
    
    // 添加token到请求头
    client.set_headers({{"Authorization", "Bearer " + token}});
    
    auto resp = client.post(url, ss.c_str(), req_content_type::json);
    std::cout << "Change password response: " << resp.resp_body << std::endl;
    
    // 验证响应
    CHECK(resp.resp_body.find("\"success\":true") != std::string::npos);
    // 验证响应 - 使用unicode_to_utf8转换后再进行判断
    std::string expected_success = "密码修改成功";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_success) != std::string::npos);
    
    // 验证新密码是否可以用于登录
    coro_http_client login_client{};
    std::string login_url{"http://127.0.0.1:3389/api/v1/login"};
    login_info login_info;
    
    login_info.username = g_username;
    login_info.password = "NewPassword123";
    
    string_stream login_ss;
    to_json(login_info, login_ss);
    
    auto login_resp = login_client.post(login_url, login_ss.c_str(), req_content_type::json);
    CHECK(login_resp.resp_body.find("\"success\":true") != std::string::npos);
  }
  
  SUBCASE("Change password with wrong old password") {
    coro_http_client client{};
    std::string url{"http://127.0.0.1:3389/api/v1/change_password"};
    change_password_info info;
    
    info.user_id = user_id;
    info.old_password = "WrongOldPassword";
    info.new_password = "NewPassword123";
    
    string_stream ss;
    to_json(info, ss);
    
    // 添加token到请求头
    client.set_headers({{"Authorization", "Bearer " + token}});
    
    auto resp = client.post(url, ss.c_str(), req_content_type::json);
    std::cout << "Change password with wrong old password response: " << resp.resp_body << std::endl;
    
    // 验证响应
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证响应 - 使用unicode_to_utf8转换后再进行判断
    std::string expected_wrong_old_pwd = "旧密码错误";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_wrong_old_pwd) != std::string::npos);
  }
}

// 测试重置密码功能
TEST_CASE("User Reset Password Tests") {
  // 首先注册测试用户
  register_test_user();
  
  // 测试请求重置密码
  SUBCASE("Request password reset") {
    coro_http_client client{};
    forgot_password_info info;
    
    info.email = g_email;
    
    string_stream ss;
    to_json(info, ss);
    
    auto resp = client.post(forget_password_url, ss.c_str(), req_content_type::json);
    std::cout << "Forgot password response: " << resp.resp_body << std::endl;
    
    // 验证响应（不区分邮箱是否存在）
    CHECK(resp.resp_body.find("\"success\":true") != std::string::npos);
    // 验证响应（不区分邮箱是否存在）- 使用unicode_to_utf8转换后再进行判断
    std::string expected_reset_sent = "密码重置链接已发送,请检查您的邮箱并完成后续操作";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_reset_sent) != std::string::npos);
  }
  
  // 注意：由于需要验证邮件中的token，完整的重置密码流程测试需要手动验证或使用模拟邮件服务
  // 这里只测试重置密码API的基本功能
  
  SUBCASE("Reset password with invalid token") {
    coro_http_client client{};
    reset_password_info info;
    
    info.token = "invalid_token_12345";
    info.new_password = "NewPassword123";
    
    string_stream ss;
    to_json(info, ss);
    
    auto resp = client.post(reset_password_url, ss.c_str(), req_content_type::json);
    std::cout << "Reset password with invalid token response: " << resp.resp_body << std::endl;
    
    // 验证响应
    CHECK(resp.resp_body.find("\"success\":false") != std::string::npos);
    // 验证响应 - 使用unicode_to_utf8转换后再进行判断
    std::string expected_invalid_token = "重置密码链接无效或已过期";
    std::string rsp_body_u8 = purecpp::escape_unicode_to_utf8(resp.resp_body.data());
    CHECK(rsp_body_u8.find(expected_invalid_token) != std::string::npos);
  }
}
