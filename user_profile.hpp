#pragma once
#include <chrono>
#include <cinttypes>
#include <format>
#include <iguana/json_reader.hpp>
#include <iguana/json_writer.hpp>
#include <string>

#include "cinatra.hpp"
#include "entity.hpp"
#include "user_aspects.hpp"
#include "user_dto.hpp"

using namespace cinatra;
using namespace ormpp;
using namespace purecpp;

struct user_profile_t {
  // 获取个人资料
  void handle_get_profile(coro_http_request& req, coro_http_response& resp) {
    // 从请求中获取用户ID
    // 假设用户已登录，user_id存储在请求的某个地方，比如cookie或session中
    // 这里简化处理，假设从请求参数中获取user_id
    auto user_id_str_view = req.get_header_value("X-User-ID");
    if (user_id_str_view.empty()) {
      // 如果没有用户ID，返回未授权错误
      rest_response<empty_data> data{};
      data.success = false;
      data.message = "未授权访问";
      data.code = 401;

      std::string json;
      iguana::to_json(data, json);
      resp.set_content_type<resp_content_type::json>();
      resp.set_status_and_content(status_type::unauthorized, std::move(json));
      return;
    }

    // 转换string_view到string，然后转换为uint64_t
    std::string user_id_str(user_id_str_view);
    uint64_t user_id = std::stoull(user_id_str);

    // 从数据库获取用户信息
    auto& db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (!conn) {
      rest_response<empty_data> data{};
      data.success = false;
      data.message = "数据库连接失败";
      data.code = 500;

      std::string json;
      iguana::to_json(data, json);
      resp.set_content_type<resp_content_type::json>();
      resp.set_status_and_content(status_type::internal_server_error,
                                  std::move(json));
      return;
    }

    profile_resp_data profile_data;
    try {
      // 使用query_s方法，它返回一个vector
      auto users = conn->query_s<users_t>("id = ?", user_id);
      if (users.empty()) {
        rest_response<empty_data> data{};
        data.success = false;
        data.message = "用户不存在";
        data.code = 404;

        std::string json;
        iguana::to_json(data, json);
        resp.set_content_type<resp_content_type::json>();
        resp.set_status_and_content(status_type::not_found, std::move(json));
        return;
      }
      auto& user = users[0];
      // 构造响应数据
      profile_data.user_id = user.id;
      profile_data.username = user.user_name.data();
      profile_data.email = user.email.data();
      profile_data.is_verifyed = user.is_verifyed;
      profile_data.title = user.title;
      profile_data.role = user.role;
      profile_data.experience = user.experience;

      profile_data.level = user.level;
      profile_data.bio = user.bio;
      profile_data.avatar = user.avatar;

      // 将时间戳转换为ISO格式
      auto created_time = std::chrono::system_clock::time_point(
          std::chrono::seconds(user.created_at));
      profile_data.created_at = std::format("{:%FT%T}", created_time);

    } catch (const std::exception& e) {
      rest_response<empty_data> data{};
      data.success = false;
      data.message = "查询用户信息失败: " + std::string(e.what());
      data.code = 500;

      std::string json;
      iguana::to_json(data, json);
      resp.set_content_type<resp_content_type::json>();
      resp.set_status_and_content(status_type::internal_server_error,
                                  std::move(json));
      return;
    }

    rest_response<profile_resp_data> data{};
    data.data = profile_data;
    data.success = true;
    data.message = "获取个人资料成功";
    data.code = 200;

    std::string json;
    iguana::to_json(data, json);
    resp.set_content_type<resp_content_type::json>();
    resp.set_status_and_content(status_type::ok, std::move(json));
    return;
  }
};
