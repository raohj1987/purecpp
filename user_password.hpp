#pragma once

#include "common.hpp"
#include "entity.hpp"
#include "user_aspects.hpp"
#include "user_register.hpp"

using namespace cinatra;

namespace purecpp {
class user_password_t {
public:
  /**
   * @brief 处理用户修改密码请求
   *
   * @param req HTTP请求对象
   * @param resp HTTP响应对象
   */
  void handle_change_password(coro_http_request &req,
                              coro_http_response &resp) {
    change_password_info info =
        std::any_cast<change_password_info>(req.get_user_data());

    // 查询数据库
    auto conn = connection_pool<dbng<mysql>>::instance().get();

    // 根据用户ID查找用户
    auto users = conn->query_s<users_t>("id = ?", info.user_id);

    if (users.empty()) {
      // 用户不存在
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("用户不存在"));
      return;
    }

    users_t user = users[0];

    // 验证旧密码
    if (user.pwd_hash != sha256_simple(info.old_password)) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("旧密码错误"));
      return;
    }

    // 更新新密码
    std::string pwd_sha = sha256_simple(info.new_password);
    user.pwd_hash = pwd_sha;
    if (conn->update<users_t>(user) != 1) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("修改密码失败"));
      return;
    }

    // 返回修改成功响应
    std::string json = make_data(change_password_resp_data{}, "密码修改成功");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp