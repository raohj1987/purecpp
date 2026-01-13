#pragma once

#include "common.hpp"
#include <vector>

using namespace cinatra;

namespace purecpp {
class tags {
public:
  void get_tags(coro_http_request &req, coro_http_response &resp) {
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }
    std::vector<tags_t> vec = conn->select(ormpp::all).from<tags_t>().collect();

    std::string json = make_data(vec, "获取标签成功");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp