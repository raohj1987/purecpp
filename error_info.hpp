#pragma once
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// 先包含必要的头文件
#include <cinatra.hpp>
#include <iguana/json_writer.hpp>

#include "user_dto.hpp"

using namespace cinatra;
using namespace iguana;

// 通用错误常量 - 使用前缀避免命名冲突
inline constexpr std::string_view PURECPP_ERROR_INVALID_JSON = "无效的JSON格式";
inline constexpr std::string_view PURECPP_ERROR_INVALID_PARAMETER = "参数无效";
inline constexpr std::string_view PURECPP_ERROR_RESOURCE_NOT_FOUND =
    "资源未找到";
inline constexpr std::string_view PURECPP_ERROR_INTERNAL_SERVER =
    "服务器内部错误";

// 用户相关错误
inline constexpr std::string_view PURECPP_ERROR_USERNAME_LENGTH =
    "用户名长度非法应改为1-20。";
inline constexpr std::string_view PURECPP_ERROR_USERNAME_CHARACTER =
    "只允许字母 (a-z, A-Z), 数字 (0-9), 下划线 (_), 连字符 (-)。";
inline constexpr std::string_view PURECPP_ERROR_PASSWORD_LENGTH =
    "密码长度不合法，长度6-20位。";
inline constexpr std::string_view PURECPP_ERROR_PASSWORD_COMPLEXITY =
    "密码至少包含大小写字母和数字。";
inline constexpr std::string_view PURECPP_ERROR_EMAIL_FORMAT =
    "邮箱格式不正确。";
inline constexpr std::string_view PURECPP_ERROR_EMAIL_EXISTS = "该邮箱已存在。";
inline constexpr std::string_view PURECPP_ERROR_USERNAME_EXISTS =
    "用户名已存在。";

// 登录相关错误
inline constexpr std::string_view PURECPP_ERROR_LOGIN_INFO_EMPTY =
    "login info is empty";
inline constexpr std::string_view PURECPP_ERROR_LOGIN_INFO_JSON =
    "login info is not a required json";
inline constexpr std::string_view PURECPP_ERROR_LOGIN_JSON_INVALID =
    "login info is not a required json";
inline constexpr std::string_view PURECPP_ERROR_LOGIN_CREDENTIALS_EMPTY =
    "用户名(邮箱)、密码不能为空。";
inline constexpr std::string_view PURECPP_ERROR_USERNAME_PASSWORD =
    "用户名或密码错误";
inline constexpr std::string_view PURECPP_ERROR_LOGIN_FAILED =
    "用户名或密码错误";
inline constexpr std::string_view PURECPP_ERROR_TOKEN_EXPIRED = "令牌已过期";
inline constexpr std::string_view PURECPP_ERROR_TOKEN_INVALID = "令牌无效";

// 注册相关错误
inline constexpr std::string_view PURECPP_ERROR_REGISTER_INFO_EMPTY =
    "register info is empty";
inline constexpr std::string_view PURECPP_ERROR_REGISTER_INFO_JSON =
    "register info is not a required json";
inline constexpr std::string_view PURECPP_ERROR_REGISTER_JSON_INVALID =
    "register info is not a required json";
inline constexpr std::string_view PURECPP_ERROR_INVALID_CPP_ANSWER =
    "答案错误，请重新计算。";
inline constexpr std::string_view PURECPP_ERROR_CPP_ANSWER_WRONG =
    "问题的答案不对。";
inline constexpr std::string_view PURECPP_ERROR_REGISTER_FAILED = "注册失败";
inline constexpr std::string_view PURECPP_ERROR_EMAIL_EMPTY = "邮箱不能为空。";

// 密码相关错误
inline constexpr std::string_view PURECPP_ERROR_RESET_PASSWORD_INFO_JSON =
    "重置密码信息格式不正确。";
inline constexpr std::string_view PURECPP_ERROR_RESET_PASSWORD_PARAMS =
    "token和新密码不能为空。";
inline constexpr std::string_view PURECPP_ERROR_PASSWORD_NEW_SAME_AS_OLD =
    "新密码不能与旧密码相同。";

// 修改密码相关错误
inline constexpr std::string_view PURECPP_ERROR_CHANGE_PASSWORD_EMPTY =
    "修改密码信息不能为空。";
inline constexpr std::string_view PURECPP_ERROR_CHANGE_PASSWORD_JSON_INVALID =
    "修改密码信息格式不正确。";
inline constexpr std::string_view
    PURECPP_ERROR_CHANGE_PASSWORD_REQUIRED_FIELDS =
        "用户ID、旧密码、新密码不能为空。";

// 忘记密码相关错误
inline constexpr std::string_view PURECPP_ERROR_FORGOT_PASSWORD_JSON_INVALID =
    "请求格式不正确。";

// 重置密码相关错误
inline constexpr std::string_view PURECPP_ERROR_RESET_PASSWORD_EMPTY =
    "重置密码信息不能为空。";
inline constexpr std::string_view PURECPP_ERROR_RESET_PASSWORD_JSON_INVALID =
    "重置密码信息格式不正确。";
inline constexpr std::string_view PURECPP_ERROR_RESET_PASSWORD_REQUIRED_FIELDS =
    "token和新密码不能为空。";
