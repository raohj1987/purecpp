#pragma once

#include "common.hpp"
#include "user_aspects.hpp"

#include <random>

using namespace cinatra;

namespace purecpp {
struct client_artilce {
  std::string_view title;
  std::string_view excerpt;
  std::string_view content;
  int tag_id;
};

struct article_page_request {
  int tag_id = 0;  // 0表示所有标签
  int user_id = 0; // 0表示所有用户
  int current_page;
  int per_page;
};

struct article_list {
  std::string title;
  std::string summary;
  std::array<char, 8> slug;
  std::array<char, 21> author_name;
  int author_id;
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

static std::string_view REVIEW_REJECTED = "rejected"; // 审核_已拒绝
static std::string_view REVIEW_ACCEPTED =
    "accepted"; // 审核_已接受(只有审核通过才会是已发布)
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
                                  make_error("无效的请求参数，请求体不能为空"));
      return;
    }

    client_artilce art{};
    std::error_code ec;
    iguana::from_json(art, body, ec);
    if (ec) {
      resp.set_status_and_content(
          status_type::bad_request,
          make_error("无效的请求参数，JSON格式错误: " + ec.message()));
      return;
    }

    // 验证标题
    if (art.title.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("标题不能为空"));
      return;
    }
    if (art.title.size() > 100) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("标题太长，不要超过100个字符"));
      return;
    }

    // 验证摘要
    if (art.excerpt.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("摘要不能为空"));
      return;
    }
    if (art.excerpt.size() > 300) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("摘要太长，不要超过300个字符"));
      return;
    }

    // 验证内容
    if (art.content.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("内容不能为空"));
      return;
    }
    if (art.content.size() > 64 * 1024) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("内容太长，不要超过64KB个字符"));
      return;
    }

    // 验证标签ID
    if (art.tag_id <= 0) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效的标签ID"));
      return;
    }

    // 从token中提取用户ID
    auto user_id = purecpp::get_user_id_from_token(req);
    if (user_id == 0) {
      resp.set_status_and_content(status_type::unauthorized,
                                  make_error("用户未登录或登录已过期"));
      return;
    }

    articles_t article{};
    article.tag_id = art.tag_id;
    article.title = art.title;
    article.abstraction = art.excerpt;
    article.content = art.content;
    article.created_at = get_timestamp_milliseconds();
    article.updated_at = get_timestamp_milliseconds();
    article.author_id = user_id;
    article.status = PENDING_REVIEW;
    article.is_deleted = false;
    article.views_count = 0;
    article.comments_count = 0;
    generate_random_string(article.slug);

    auto conn = connection_pool<dbng<mysql>>::instance().get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    int retry = 5;
    uint64_t article_id = 0;
    for (; retry > 0; retry--) {
      article_id = conn->get_insert_id_after_insert(article);
      if (article_id > 0) {
        break;
      }
      generate_random_string(article.slug);
    }

    if (retry == 0) {
      auto err = conn->get_last_error();
      CINATRA_LOG_ERROR << "提交文章失败: " << err;
      set_server_internel_error(resp);
      return;
    }

    resp.set_status_and_content(status_type::ok,
                                make_success("文章提交成功，等待审核"));
  }

  void show_article(coro_http_request &req, coro_http_response &resp) {
    auto it = req.params_.find("slug");
    if (it == req.params_.end()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效的请求参数，缺少文章标识符"));
      return;
    }

    auto slug = it->second;
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    // 先更新浏览量
    articles_t article;
    article.views_count = 1;
    conn->update_some<&articles_t::views_count>(
        article, "slug='" + std::string(slug) + "'",
        "views_count = views_count + 1");

    // 再获取文章详情
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

    if (!list.empty()) {
      std::string json = make_data(std::move(list[0]), "获取文章详情成功");
      resp.set_status_and_content(status_type::ok, std::move(json));
    } else {
      resp.set_status_and_content(status_type::not_found,
                                  make_error("文章不存在或已被删除"));
    }
  }

  void edit_article(coro_http_request &req, coro_http_response &resp) {
    edit_article_info info =
        std::any_cast<edit_article_info>(req.get_user_data());
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    articles_t article{};
    article.tag_id = info.tag_id;
    article.title = info.title;
    article.abstraction = info.excerpt;
    article.content = info.content;
    article.status = PENDING_REVIEW.data();
    article.updated_at = get_timestamp_milliseconds();

    // 使用安全的字符串拼接，避免SQL注入风险
    std::string slug = "slug='";
    slug.append(info.slug).append("'");
    int n = conn->update_some<&articles_t::tag_id, &articles_t::title,
                              &articles_t::abstraction, &articles_t::content,
                              &articles_t::status, &articles_t::updated_at>(
        article, slug);

    if (n == 0) {
      set_server_internel_error(resp);
      return;
    }
    std::string json = make_success("修改成功");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  void get_articles(coro_http_request &req, coro_http_response &resp) {
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }

    // 从请求体中获取分页信息
    auto body = req.get_body();
    article_page_request page_req{};
    std::error_code ec;
    iguana::from_json(page_req, body, ec);
    if (ec) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效的请求参数"));
      return;
    }

    int page = 1;
    int per_page = 10;

    if (page_req.current_page > 0) {
      page = page_req.current_page;
    }
    if (page_req.per_page > 0 && page_req.per_page <= 50) { // 限制每页最多50条
      per_page = page_req.per_page;
    }
    // 获取文章列表
    auto where_cond = col(&articles_t::is_deleted) == 0 &&
                      col(&articles_t::status) == PUBLISHED.data();
    if (page_req.tag_id > 0) {
      where_cond = where_cond && (col(&articles_t::tag_id) == page_req.tag_id);
    }
    if (page_req.user_id > 0) {
      where_cond =
          where_cond && (col(&articles_t::author_id) == page_req.user_id);
    }
    auto select_cond =
        conn->select(col(&articles_t::title), col(&articles_t::abstraction),
                     col(&articles_t::slug), col(&users_t::user_name),
                     col(&articles_t::author_id), col(&tags_t::name),
                     col(&articles_t::created_at), col(&articles_t::updated_at),
                     col(&articles_t::views_count),
                     col(&articles_t::comments_count))
            .from<articles_t>()
            .inner_join(col(&articles_t::author_id), col(&users_t::id))
            .inner_join(col(&articles_t::tag_id), col(&tags_t::tag_id))
            .where(where_cond);

    // 计算总记录数(根据查询条件)
    size_t total_count =
        conn->select(ormpp::count())
            .from<articles_t>()
            .inner_join(col(&articles_t::author_id), col(&users_t::id))
            .inner_join(col(&articles_t::tag_id), col(&tags_t::tag_id))
            .where(where_cond)
            .collect();

    size_t limit = per_page;
    size_t offset = (page - 1) * per_page;
    auto list = select_cond.order_by(col(&articles_t::created_at).desc())
                    .limit(ormpp::token)
                    .offset(ormpp::token)
                    .collect<article_list>(limit, offset);

    std::string json =
        make_data(std::move(list), "获取文章列表成功", total_count);
    if (json.empty()) {
      set_server_internel_error(resp);
      return;
    }

    resp.set_status_and_content(status_type::ok, std::move(json));
  }

  void get_pending_articles(coro_http_request &req, coro_http_response &resp) {
    auto conn = connection_pool<dbng<mysql>>::instance().get();
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
                   col(&articles_t::status) == PENDING_REVIEW.data())
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
                                  make_error("无效的请求参数，请求体不能为空"));
      return;
    }

    review_opinion op{};
    std::error_code ec;
    iguana::from_json(op, body, ec);
    if (ec) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效的请求参数，JSON格式错误"));
      return;
    }
    auto conn = connection_pool<dbng<mysql>>::instance().get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }
    // 检查审核人是否是管理员
    auto user_id = get_user_id_from_token(req);
    if (user_id == 0) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效的请求参数"));
      return;
    }
    auto users_vect = conn->select(ormpp::all)
                          .from<users_t>()
                          .where(col(&users_t::id) == user_id)
                          .collect();
    if (users_vect.empty()) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效的请求参数"));
      return;
    }

    auto &review_user = users_vect.front();
    if (review_user.role != "admin" && review_user.role != "superadmin") {
      resp.set_status_and_content(
          status_type::bad_request,
          make_error("无效的请求参数，审核人必须是管理员"));
      return;
    }
    // 检查审核人名称是否匹配
    if (op.reviewer_name.empty() &&
        strcmp(op.reviewer_name.data(), review_user.user_name.data()) != 0) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效的请求参数，审核人不能为空"));
      return;
    }
    // 检查审核结论
    if (op.review_status != REVIEW_ACCEPTED &&
        op.review_status != REVIEW_REJECTED) {
      resp.set_status_and_content(status_type::bad_request,
                                  make_error("无效的请求参数，审核状态必须是" +
                                             std::string(REVIEW_ACCEPTED) +
                                             "或" +
                                             std::string(REVIEW_REJECTED)));
      return;
    }

    articles_t article{};
    article.reviewer_id = review_user.id;
    article.review_date = get_timestamp_milliseconds();
    article.status = op.review_status == REVIEW_ACCEPTED ? PUBLISHED : REJECTED;

    // 使用安全的字符串拼接，避免SQL注入风险
    std::string slug = "slug='";
    slug.append(op.slug).append("'");
    int n =
        conn->update_some<&articles_t::reviewer_id, &articles_t::review_date,
                          &articles_t::status>(article, slug);
    if (n == 0) {
      set_server_internel_error(resp);
      return;
    }
    std::string json = make_success("审核成功");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp
