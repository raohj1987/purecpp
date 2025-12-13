#pragma once

#include "common.hpp"
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

struct article_detail {
  std::string title;
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

    articles_t article{};
    article.tag_id = art.tag_id;
    article.title = art.title;
    article.abstraction = art.excerpt;
    article.content = art.content;
    article.created_at = get_timestamp_milliseconds();
    article.author_id = 1; // only for test
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
      std::cout << err << "\n";
      set_server_internel_error(resp);
      return;
    }

    resp.set_status_and_content(status_type::ok, "提交审核成功");
  }

  void get_article_count(coro_http_request &req, coro_http_response &resp) {
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    auto counts = conn->query_s<std::tuple<int64_t>>(
        "select count(1) from articles t where t.is_deleted = 0");
    size_t count = 0;
    if (!counts.empty()) {
      count = std::get<0>(counts[0]);
    }

    resp.set_status_and_content(status_type::ok, make_data(count));
  }

  void show_article(coro_http_request &req, coro_http_response &resp) {
    auto it = req.params_.find("slug");
    if (it == req.params_.end()) {
      resp.set_status_and_content(status_type::bad_request, "error");
      return;
    }

    auto slug = it->second;
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    std::string sql =
        "select a.title, a.content, u.user_name as author_name, t.name as "
        "tag_name, a.created_at, a.updated_at, a.views_count, a.comments_count "
        "from articles a INNER JOIN users u ON a.author_id = u.id INNER JOIN "
        "tags t ON a.tag_id = t.tag_id WHERE a.slug = ? and a.is_deleted=0;";

    // article_detail
    auto list = conn->query_s<article_detail>(sql, slug);
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

  void get_articles(coro_http_request &req, coro_http_response &resp) {
    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    std::string sql =
        "SELECT  a.title, a.abstraction AS article_summary, "
        "a.slug, "
        "u.user_name AS author_name, t.name AS tag_name, a.created_at, "
        "a.updated_at, a.views_count, a.comments_count FROM articles a INNER "
        "JOIN  users u ON a.author_id = u.id INNER JOIN  tags t ON a.tag_id = "
        "t.tag_id WHERE  a.is_deleted = 0 ORDER BY  a.created_at DESC LIMIT 20 "
        "OFFSET ?;";

    auto list = conn->query_s<article_list>(sql, 0);

    std::string json = make_data(std::move(list));
    if (json.empty()) {
      set_server_internel_error(resp);
      return;
    }

    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp