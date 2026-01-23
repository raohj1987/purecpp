#pragma once

#include "common.hpp"
#include "user_aspects.hpp"
#include "user_experience_aspects.hpp"

#include <random>

using namespace cinatra;

namespace purecpp {
struct client_artilce {
  std::string_view title;
  std::string_view excerpt;
  std::string_view content;
  int tag_id;
};

struct article_list {
  std::string title;
  std::string summary;
  std::array<char, 8> slug;
  std::array<char, 21> author_name;
  std::array<char, 50> tag_name;
  uint64_t created_at;
  uint64_t updated_at;
  uint32_t views_count;
  uint32_t comments_count;
};

struct pending_article_list {
  std::string title;
  std::string summary;
  std::string content;
  std::array<char, 8> slug;
  std::array<char, 21> author_name;
  std::array<char, 50> tag_name;
  uint64_t created_at;
  uint64_t updated_at;
  uint32_t views_count;
  uint32_t comments_count;
};

struct review_opinion {
  std::string reviewer_name;
  std::string slug;
  std::string review_status;
};

struct article_detail {
  std::string title;
  std::string summary;
  std::string content;
  std::array<char, 21> author_name;
  std::array<char, 50> tag_name;
  uint64_t created_at;
  uint64_t updated_at;
  uint32_t views_count;
  uint32_t comments_count;
};

struct comments {
  std::array<char, 21> author_name;
  std::array<char, 21> parant_name;
  std::string_view content;
  uint64_t created_at;
  uint64_t updated_at;
};

inline void generate_random_string(auto &random_str) {
  static const std::string chars = "abcdefghijklmnopqrstuvwxyz"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "0123456789";

  static std::random_device rd;

  static std::mt19937 generator(rd());

  static std::uniform_int_distribution<> distribution(0, chars.length() - 1);

  for (std::size_t i = 0; i < random_str.size(); ++i) {
    random_str[i] = chars[distribution(generator)];
  }
}

class articles {
public:
  void handle_new_article(coro_http_request &req, coro_http_response &resp) {
    auto body = req.get_body();
    if (body.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("empty body"));
      return;
    }

    client_artilce art{};
    std::error_code ec;
    iguana::from_json(art, body, ec);
    if (ec) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error(ec.message()));
      return;
    }

    if (art.title.size() > 100) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("标题太长，不要超过100个字符"));
      return;
    }

    if (art.excerpt.size() > 300) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("摘要太长，不要超过300个字符"));
      return;
    }

    if (art.content.size() > 64 * 1024) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("内容太长，不要超过64KB个字符"));
      return;
    }
    // 从token中提取用户ID
    auto user_id = purecpp::get_user_id_from_token(req);
    if (user_id == 0) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("error"));
      return;
    }

    articles_t article{};
    article.tag_id = art.tag_id;
    article.title = art.title;
    article.abstraction = art.excerpt;
    article.content = art.content;
    article.created_at = get_timestamp_milliseconds();
    article.author_id = user_id;
    generate_random_string(article.slug);

    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    int retry = 5;
    for (; retry > 0; retry--) {
      auto id = conn->get_insert_id_after_insert(article);
      if (id > 0) {
        break;
      }

      generate_random_string(article.slug);
    }

    if (retry == 0) {
      auto err = conn->get_last_error();
      CINATRA_LOG_ERROR << err;
      set_server_internel_error(resp);
      return;
    }

    resp.set_status_and_content(status_type::ok, make_data("提交审核成功"));
  }

  void get_article_count(coro_http_request &req, coro_http_response &resp) {
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    size_t count = conn->select(ormpp::count())
                       .from<articles_t>()
                       .where(col(&articles_t::is_deleted) == 0)
                       .collect();

    resp.set_status_and_content(status_type::ok, make_data(count));
  }

  void show_article(coro_http_request &req, coro_http_response &resp) {
    auto it = req.params_.find("slug");
    if (it == req.params_.end()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("error"));
      return;
    }

    auto slug = it->second;
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    // article_detail
    auto list =
        conn->select(col(&articles_t::title), col(&articles_t::abstraction),
                     col(&articles_t::content), col(&users_t::user_name),
                     col(&tags_t::name), col(&articles_t::created_at),
                     col(&articles_t::updated_at),
                     col(&articles_t::views_count),
                     col(&articles_t::comments_count))
            .from<articles_t>()
            .inner_join(col(&articles_t::author_id), col(&users_t::id))
            .inner_join(col(&articles_t::tag_id), col(&tags_t::tag_id))
            .where(col(&articles_t::slug).param() &&
                   col(&articles_t::is_deleted) == 0)
            .collect<article_detail>(slug);

    std::string json;
    if (!list.empty()) {
      json = make_data(std::move(list[0]));
      if (json.empty()) {
        set_server_internel_error(resp);
        return;
      }
    }

    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  void edit_article(coro_http_request &req, coro_http_response &resp) {
    edit_article_info info =
        std::any_cast<edit_article_info>(req.get_user_data());
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    articles_t article{};
    article.tag_id = info.tag_id;
    article.title = info.title;
    article.abstraction = info.excerpt;
    article.content = info.content;
    article.updated_at = get_timestamp_milliseconds();

    std::string slug = "slug='";
    slug.append(info.slug).append("'");
    int n = conn->update_some<&articles_t::tag_id, &articles_t::title,
                              &articles_t::abstraction, &articles_t::content,
                              &articles_t::updated_at>(article, slug);

    if (n == 0) {
      set_server_internel_error(resp);
      return;
    }
    std::string json = make_data(std::string("修改成功"));
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  void get_articles(coro_http_request &req, coro_http_response &resp) {
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    size_t limit = 20; // will update, it's from web front end.
    size_t offset = 0; // will update, it's from web front end.

    auto list =
        conn->select(col(&articles_t::title), col(&articles_t::abstraction),
                     col(&articles_t::slug), col(&users_t::user_name),
                     col(&tags_t::name), col(&articles_t::created_at),
                     col(&articles_t::updated_at),
                     col(&articles_t::views_count),
                     col(&articles_t::comments_count))
            .from<articles_t>()
            .inner_join(col(&articles_t::author_id), col(&users_t::id))
            .inner_join(col(&articles_t::tag_id), col(&tags_t::tag_id))
            .where(col(&articles_t::is_deleted) == 0 &&
                   col(&articles_t::review_status) == "accepted")
            .order_by(col(&articles_t::created_at).desc())
            .limit(ormpp::token)
            .offset(ormpp::token)
            .collect<article_list>(limit, offset);

    std::string json = make_data(std::move(list));
    if (json.empty()) {
      set_server_internel_error(resp);
      return;
    }

    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  void get_pending_articles(coro_http_request &req, coro_http_response &resp) {
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    size_t limit = 20; // will update, it's from web front end.
    size_t offset = 0; // will update, it's from web front end.

    auto list =
        conn->select(col(&articles_t::title), col(&articles_t::abstraction),
                     col(&articles_t::content), col(&articles_t::slug),
                     col(&users_t::user_name), col(&tags_t::name),
                     col(&articles_t::created_at), col(&articles_t::updated_at),
                     col(&articles_t::views_count),
                     col(&articles_t::comments_count))
            .from<articles_t>()
            .inner_join(col(&articles_t::author_id), col(&users_t::id))
            .inner_join(col(&articles_t::tag_id), col(&tags_t::tag_id))
            .where(col(&articles_t::is_deleted) == 0 &&
                   col(&articles_t::review_status) == "pending_review")
            .order_by(col(&articles_t::created_at).desc())
            .limit(ormpp::token)
            .offset(ormpp::token)
            .collect<pending_article_list>(limit, offset);

    std::string json = make_data(std::move(list));
    if (json.empty()) {
      set_server_internel_error(resp);
      return;
    }

    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  void handle_review_article(coro_http_request &req, coro_http_response &resp) {
    auto body = req.get_body();
    if (body.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  "invalid request parameter");
      return;
    }

    review_opinion op{};
    std::error_code ec;
    iguana::from_json(op, body, ec);
    if (ec) {
      resp.set_status_and_content(status_type::bad_request,
                                  "invalid request parameter");
      return;
    }

    if (op.review_status != "accepted" && op.review_status != "rejected") {
      resp.set_status_and_content(status_type::bad_request,
                                  "invalid request parameter");
      return;
    }

    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    auto vec = conn->select(col(&users_t::id))
                   .from<users_t>()
                   .where(col(&users_t::user_name).param() &&
                          col(&users_t::role) == "admin")
                   .collect(op.reviewer_name);
    if (vec.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("invalid request parameter"));
      return;
    }

    auto id = std::get<0>(vec.front());

    articles_t article{};
    article.reviewer_id = id;
    article.review_date = get_timestamp_milliseconds();
    article.review_status = op.review_status;

    std::string slug = "slug='";
    slug.append(op.slug).append("'");
    int n =
        conn->update_some<&articles_t::reviewer_id, &articles_t::review_date,
                          &articles_t::review_status>(article, slug);
    if (n == 0) {
      set_server_internel_error(resp);
      return;
    }
    std::string json = make_data(std::string("审核成功"));
    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp
