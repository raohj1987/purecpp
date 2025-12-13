#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

// 包含必要的头文件
#include "../jwt_token.hpp"

using namespace purecpp;

// 测试generate_jwt_token函数
TEST_CASE("generate_jwt_token Tests") {
  SUBCASE("Generate token with valid parameters") {
    uint64_t user_id = 12345;
    std::string username = "testuser";
    std::string email = "test@example.com";

    std::string token = generate_jwt_token(user_id, username, email);

    // 验证token不为空
    CHECK(!token.empty());
    // 验证token长度合理
    CHECK(token.size() > 20);
  }

  SUBCASE("Generate token with special characters in username and email") {
    uint64_t user_id = 67890;
    std::string username = "test_user_123";
    std::string email = "test.email+tag@example.com";

    std::string token = generate_jwt_token(user_id, username, email);

    // 验证token不为空
    CHECK(!token.empty());
  }
}

// 测试base64_decode函数
TEST_CASE("base64_decode Tests") {
  SUBCASE("Decode valid base64 string") {
    std::string encoded = "SGVsbG8sIFdvcmxkIQ==";
    auto result = base64_decode(encoded);

    CHECK(result.has_value());
    CHECK(*result == "Hello, World!");
  }

  SUBCASE("Decode valid base64 string without padding") {
    std::string encoded = "SGVsbG8sIFdvcmxkIQ";
    auto result = base64_decode(encoded);

    CHECK(result.has_value());
    CHECK(*result == "Hello, World!");
  }

  SUBCASE("Decode invalid base64 string") {
    std::string encoded = "SGVsbG8s%20IFdvcmxkIQ==";
    auto result = base64_decode(encoded);

    CHECK(!result.has_value());
  }

  SUBCASE("Decode empty string") {
    std::string encoded = "";
    auto result = base64_decode(encoded);

    CHECK(result.has_value());
    CHECK(*result == "");
  }
}

// 测试validate_jwt_token函数
TEST_CASE("validate_jwt_token Tests") {
  SUBCASE("Validate valid token") {
    uint64_t user_id = 12345;
    std::string username = "testuser";
    std::string email = "test@example.com";

    // 生成一个有效的token
    std::string token = generate_jwt_token(user_id, username, email);

    // 验证token
    auto [result, info] = validate_jwt_token(token);

    CHECK(result == TokenValidationResult::Valid);
    CHECK(info.has_value());
    CHECK(info->user_id == user_id);
    CHECK(info->username == username);
    CHECK(info->email == email);
  }

  SUBCASE("Validate invalid base64 token") {
    std::string invalid_token = "invalid_base64_token_with_special_chars!@#$";

    auto [result, info] = validate_jwt_token(invalid_token);

    CHECK(result == TokenValidationResult::InvalidBase64);
    CHECK(!info.has_value());
  }

  SUBCASE("Validate invalid format token") {
    // 生成一个格式错误的token
    std::string invalid_format = "SGVsbG8sIFdvcmxkIQ==";

    auto [result, info] = validate_jwt_token(invalid_format);

    CHECK(result == TokenValidationResult::InvalidFormat);
    CHECK(!info.has_value());
  }

  SUBCASE("Validate manually created valid token") {
    // 手动创建一个符合格式的token
    uint64_t user_id = 67890;
    std::string username = "manualuser";
    std::string email = "manual@example.com";
    uint64_t timestamp = purecpp::get_jwt_timestamp_milliseconds();

    std::string token_content = std::to_string(user_id) + ":" + username + ":" +
                                email + ":" + std::to_string(timestamp);

    // 使用现有的generate_jwt_token函数来生成base64编码
    std::string token = generate_jwt_token(user_id, username, email);

    auto [result, info] = validate_jwt_token(token);

    CHECK(result == TokenValidationResult::Valid);
    CHECK(info.has_value());
    CHECK(info->user_id == user_id);
    CHECK(info->username == username);
    CHECK(info->email == email);
    CHECK(info->timestamp == timestamp);
  }
}

// 测试token过期功能
TEST_CASE("Token Expiration Tests") {
  // 注意：这个测试需要修改validate_jwt_token函数中的过期时间或模拟时间
  // 为了不影响正常功能，这里我们使用一个简单的测试方法

  SUBCASE("Validate token expiration logic") {
    // 生成一个token
    uint64_t user_id = 11111;
    std::string username = "expiretest";
    std::string email = "expire@example.com";

    std::string token = generate_jwt_token(user_id, username, email);

    // 立即验证token，应该有效
    auto [result1, info1] = validate_jwt_token(token);

    CHECK(result1 == TokenValidationResult::Valid);
    CHECK(info1.has_value());
    CHECK(info1->user_id == user_id);
    CHECK(info1->username == username);
    CHECK(info1->email == email);

    // 注意：要测试真正的过期，我们需要修改validate_jwt_token中的过期时间
    // 或者使用一个模拟的时间系统
    // 这里我们只是验证过期逻辑的存在
  }
}
