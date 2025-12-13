#pragma once
#include <cinttypes>
#include <optional>
#include <string>
#include <vector>
#include <system_error>

#include "entity.hpp" // 包含必要的枚举类型

namespace purecpp {
    // 注册相关结构体
    struct register_info {
        std::string username;
        std::string email;
        std::string password;
        std::string cpp_answer;
        size_t question_index;
    };

    // 用户响应数据结构
    struct user_resp_data {
        uint64_t user_id;
        std::string username;
        std::string email;
        bool is_verifyed;
        UserTitle title;
        std::string role;
        uint64_t experience;
        UserLevel level;
    };

    // 登录相关结构体
    struct login_info {
        std::string username;
        std::string password;
    };

    struct login_resp_data {
        uint64_t user_id;
        std::string username;
        std::string email;
        std::string token;
        UserTitle title;
        std::string role;
        uint64_t experience;
        UserLevel level;
    };

    // 修改密码相关结构体
    struct change_password_info {
        uint64_t user_id;
        std::string old_password;
        std::string new_password;
    };

    struct change_password_resp_data {
        bool success;
        std::string message;
    };

    // 忘记密码相关结构体
    struct forgot_password_info {
        std::string email;
    };

    struct reset_password_info {
        std::string token;
        std::string new_password;
    };

    // 空数据结构体，用于没有具体数据的响应
    struct empty_data {
    };

    // 个人资料相关结构体
    struct profile_resp_data {
        uint64_t user_id;
        std::string username;
        std::string email;
        bool is_verifyed;
        UserTitle title;
        std::string role;
        uint64_t experience;
        UserLevel level;
        std::optional<std::string> bio;
        std::optional<std::string> avatar;
        std::optional<std::string> created_at;
    };
} // namespace purecpp