#pragma once
#include "articles_aspects.hpp"
#include "articles_dto.hpp"
#include "common.hpp"

#include <string>
#include <vector>

#include <cinatra.hpp>

using namespace cinatra;

namespace purecpp {

class articles_comment {
public:
  // 获取文章评论
  void get_article_comment(coro_http_request &req, coro_http_response &resp) {
    auto request = std::any_cast<get_comments_request>(req.get_user_data());

    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    // 获取文章id
    auto article_vec = conn->select(col(&articles_t::article_id))
                           .from<articles_t>()
                           .where(col(&articles_t::slug).param())
                           .collect(request.slug);
    if (article_vec.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("article not found"));
      return;
    }

    uint64_t article_id = std::get<0>(article_vec.front());

    // 获取评论列表
    auto comments =
        conn->select(col(&article_comments_t::comment_id),
                     col(&article_comments_t::article_id),
                     col(&article_comments_t::user_id),
                     col(&users_t::user_name),
                     col(&article_comments_t::content),
                     col(&article_comments_t::parent_id),
                     col(&article_comments_t::ip),
                     col(&article_comments_t::created_at),
                     col(&article_comments_t::updated_at))
            .from<article_comments_t>()
            .inner_join(col(&article_comments_t::user_id), col(&users_t::id))
            .where(col(&article_comments_t::article_id) == article_id)
            .order_by(col(&article_comments_t::created_at).desc())
            .collect<comment_response>();

    std::string json =
        make_data(comments, std::string("Comments retrieved successfully"));
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  // 添加文章评论
  void add_article_comment(coro_http_request &req, coro_http_response &resp) {
    auto request = std::any_cast<add_comment_request>(req.get_user_data());

    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    uint64_t now = get_timestamp_milliseconds();

    // 检查用户是否存在
    auto user_vec = conn->select(col(&users_t::id))
                        .from<users_t>()
                        .where(col(&users_t::user_name).param())
                        .collect(request.author_name);
    if (user_vec.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("invalid user"));
      return;
    }

    uint64_t user_id = std::get<0>(user_vec.front());

    // 检查文章是否存在
    auto article_vec = conn->select(col(&articles_t::article_id))
                           .from<articles_t>()
                           .where(col(&articles_t::slug).param())
                           .collect(request.slug);
    if (article_vec.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("article not found"));
      return;
    }

    uint64_t article_id = std::get<0>(article_vec.front());

    // 获取客户端IP地址
    auto client_ip = get_client_ip(req);

    // 插入评论
    article_comments_t new_comment{.comment_id = 0,
                                   .article_id = article_id,
                                   .user_id = user_id,
                                   .content = request.content,
                                   .parent_id = request.parent_id,
                                   .ip = {},
                                   .created_at = now,
                                   .updated_at = now};
    // 复制IP地址到std::array
    std::copy_n(client_ip.data(),
                std::min(client_ip.size(), new_comment.ip.size()),
                new_comment.ip.data());
    // 插入评论
    auto comment_id = conn->get_insert_id_after_insert(new_comment);
    if (comment_id <= 0) {
      set_server_internel_error(resp);
      return;
    }
    new_comment.comment_id = comment_id;

    // 更新文章评论计数
    auto total_comment =
        conn->select(count())
            .from<article_comments_t>()
            .where(col(&article_comments_t::article_id).param())
            .collect(article_id);
    articles_t update_article;
    update_article.comments_count = total_comment;
    std::string condition = "article_id=" + std::to_string(article_id);
    conn->update_some<&articles_t::comments_count>(update_article, condition);
    // 返回新评论信息
    comment_response response{
        .comment_id = new_comment.comment_id,
        .article_id = new_comment.article_id,
        .user_id = new_comment.user_id,
        .author_name = request.author_name,
        .content = new_comment.content,
        .parent_id = new_comment.parent_id,
        .ip = std::string(new_comment.ip.begin(), new_comment.ip.end()),
        .created_at = new_comment.created_at,
        .updated_at = new_comment.updated_at};

    std::string json = make_data(response);
    if (json.empty()) {
      set_server_internel_error(resp);
      return;
    }

    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp
