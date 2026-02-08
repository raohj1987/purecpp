#pragma once
#include <cstdint>
#include <string>

namespace purecpp {
// 获取我的文章请求结构体
struct my_article_request {
  uint64_t user_id = 0; // 0表示所有用户
  int current_page;
  int per_page;
};
// 我的文章响应item
struct my_article_item {
  uint64_t article_id;
  std::string title;
  std::string abstraction;
  std::string content;
  std::string slug;
  std::string status;
  uint64_t created_at;
  uint64_t updated_at;
  uint32_t views_count;
  uint32_t comments_count;
  std::string review_comment; // 审核结果
};

// 获取评论请求结构体
struct get_comments_request {
  std::string slug;
};
// 查询评论应答
struct get_comments_response {
  uint64_t comment_id;
  uint64_t article_id;
  uint64_t user_id;
  std::string author_name;
  std::string content;
  uint64_t parent_comment_id;   // 父级评论id
  std::string parent_user_name; // 父级用户名
  std::string ip;               // 评论者IP地址
  int32_t comment_status;       // 评论状态
  uint64_t created_at;
  uint64_t updated_at;
};

// 增加评论请求结构体
struct add_comment_request {
  std::string content;
  uint64_t parent_comment_id = 0;
  std::string author_name;
  std::string slug; // 文章对应的随机数
};

// 增加评论的响应结构体
struct add_comment_response {
  uint64_t comment_id;
  uint64_t article_id;
  uint64_t user_id;
  std::string author_name;
  std::string content;
  uint64_t parent_comment_id;   // 父级评论id
  std::string parent_user_name; // 父级用户名
  std::string ip;               // 评论者IP地址
  uint64_t created_at;
  uint64_t updated_at;
};

// 获取我的评论响应
struct user_comment_item {
  uint64_t comment_id;
  uint64_t article_id;
  std::string article_title;
  std::string content;
  uint64_t parent_comment_id;
  std::string parent_user_name;
  uint64_t created_at;
  uint64_t updated_at;
};

// 构建统计数据响应
struct stats_data {
  int user_count;
  int article_count;
};

} // namespace purecpp