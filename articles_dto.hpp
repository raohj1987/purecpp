#pragma once
#include <cstdint>
#include <string>

namespace purecpp {
// 评论相关结构体
struct comment_data {
  std::string content;
  uint64_t parent_id = 0;
  std::string author_name;
  uint64_t article_id;
};

// 获取评论请求结构体
struct get_comments_request {
  std::string slug;
};

// 增加评论请求结构体
struct add_comment_request {
  std::string content;
  uint64_t parent_id = 0;
  std::string author_name;
  std::string slug;
};

// 评论相关的响应结构体
struct comment_response {
  uint64_t comment_id;
  uint64_t article_id;
  uint64_t user_id;
  std::string author_name;
  std::string content;
  uint64_t parent_id;
  std::string ip; // 评论者IP地址
  uint64_t created_at;
  uint64_t updated_at;
};

} // namespace purecpp