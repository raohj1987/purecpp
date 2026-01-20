#pragma once

#include "entity.hpp"
#include "user_aspects.hpp"

#include <cinatra.hpp>

using namespace cinatra;

namespace purecpp {

// 用户个人信息服务类
class user_profile_t {
public:
  /**
   * @brief 获取用户的个人信息，支持通过user_id或username查询
   */
  void get_user_profile(coro_http_request &req, coro_http_response &resp) {
    // 从请求中获取用户ID或用户名
    auto body = req.get_body();

    // 定义可以接收user_id或username的请求结构体
    struct profile_request {
      uint64_t user_id = 0;
      std::string username;
    };

    profile_request request;
    std::error_code ec;
    iguana::from_json(request, body, ec);
    if (ec) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error(ec.message()));
      return;
    }

    // 用户id和username不能同时为空
    if (request.user_id == 0 && request.username.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("用户ID或用户名不能为空"));
      return;
    }

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    // 查询用户信息
    std::vector<users_t> users;
    if (request.user_id != 0) {
      // 通过user_id查询
      users = conn->select(ormpp::all)
                  .from<users_t>()
                  .where(col(&users_t::id).param())
                  .collect(request.user_id);
    } else {
      // 通过username查询
      users = conn->select(ormpp::all)
                  .from<users_t>()
                  .where(col(&users_t::user_name).param())
                  .collect(request.username);
    }

    if (users.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("用户不存在"));
      return;
    }

    auto &user = users[0];

    // 构建响应
    get_profile_response profile;
    profile.username = std::string(user.user_name.data());
    profile.email = std::string(user.email.data());
    profile.location = user.location;
    profile.bio = user.bio;
    profile.avatar = user.avatar;
    profile.skills = user.skills;
    profile.created_at = user.created_at;
    profile.last_active_at = user.last_active_at;
    profile.title = user.title;
    profile.role = user.role;
    profile.experience = user.experience;
    profile.level = user.level;
    profile.status = user.status;

    std::string json = make_data(profile, "获取用户信息成功");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  /**
   * @brief 更新当前用户的个人信息
   */
  void update_user_profile(coro_http_request &req, coro_http_response &resp) {
    // 从请求体中获取更新信息
    auto body = req.get_body();
    user_profile_request update_info;
    std::error_code ec;
    iguana::from_json(update_info, body, ec);
    if (ec) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error(ec.message()));
      return;
    }

    // 用户id不能不能为空
    if (update_info.user_id == 0) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("用户ID不能为空"));
      return;
    }

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    // 获取现有用户信息
    auto users = conn->select(ormpp::all)
                     .from<users_t>()
                     .where(col(&users_t::id).param())
                     .collect(update_info.user_id);

    if (users.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("用户不存在"));
      return;
    }

    users_t user = users[0];

    // 更新字段
    if (update_info.location.has_value()) {
      user.location = update_info.location;
    }
    if (update_info.bio.has_value()) {
      user.bio = update_info.bio;
    }
    if (update_info.avatar.has_value()) {
      user.avatar = update_info.avatar;
    }
    if (update_info.skills.has_value()) {
      user.skills = update_info.skills;
    }

    // 更新数据库
    if (conn->update<users_t>(user) != 1) {
      resp.set_status_and_content(status_type::internal_server_error,
                                  make_error("更新用户信息失败"));
      return;
    }

    resp.set_status_and_content(status_type::ok,
                                make_data(empty_data{}, "更新用户信息成功"));
  }
};

} // namespace purecpp
