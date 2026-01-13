#pragma once
#include "common.hpp"
#include "entity.hpp"
#include <any>
#include <chrono>
#include <string_view>

#include "articles_dto.hpp"
#include <cinatra.hpp>
#include <iguana/json_reader.hpp>
#include <iguana/json_writer.hpp>

using namespace cinatra;
using namespace iguana;

namespace purecpp {

// 合并的获取评论校验切面
struct check_get_comments {
  bool before(coro_http_request &req, coro_http_response &res) {
    // 1. 检查获取评论请求的输入参数
    auto it = req.params_.find("slug");
    if (it == req.params_.end()) {
      res.set_status_and_content(status_type::bad_request, "invalid slug");
      return false;
    }

    auto slug = it->second;

    get_comments_request request;
    request.slug = slug;
    req.set_user_data(request);
    return true;
  }
};

// 合并的添加评论校验切面
struct check_add_comment {
  bool before(coro_http_request &req, coro_http_response &res) {
    // 1. 检查添加评论请求的输入参数
    auto body = req.get_body();
    if (body.empty()) {
      res.set_status_and_content(status_type::bad_request,
                                 "invalid request body");
      return false;
    }

    add_comment_request request;
    std::error_code ec;
    iguana::from_json(request, body, ec);
    if (ec) {
      res.set_status_and_content(status_type::bad_request,
                                 "invalid request parameter");
      return false;
    }

    // 检查必填字段
    if (request.content.empty() || request.author_name.empty() ||
        request.slug.empty()) {
      res.set_status_and_content(status_type::bad_request,
                                 "missing required fields");
      return false;
    }

    req.set_user_data(request);
    return true;
  }
};

} // namespace purecpp
