#pragma once

#include "common.hpp"
#include "user_aspects.hpp"
#include <openssl/sha.h>

using namespace cinatra;

namespace purecpp {
inline std::string sha256_simple(std::string_view input) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(input.data()), input.size(),
         hash);

  std::string hex(SHA256_DIGEST_LENGTH * 2, '\0');

  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    char buf[2];

    // 高4位
    std::to_chars(buf, buf + 1, hash[i] >> 4, 16);
    hex[i * 2] = buf[0];

    // 低4位
    std::to_chars(buf, buf + 1, hash[i] & 0x0F, 16);
    hex[i * 2 + 1] = buf[0];
  }

  return hex;
}

class user_register_t {
public:
  void handle_register(coro_http_request &req, coro_http_response &resp) {
    register_info info = std::any_cast<register_info>(req.get_user_data());

    // save to database
    users_t user{.id = 0,
                 .status = "Offline",
                 .is_verifyed = false,
                 .created_at = get_timestamp_milliseconds(),
                 .last_active_at = 0};
    std::string pwd_sha = sha256_simple(info.password);
    user.pwd_hash = pwd_sha;
    std::copy(info.username.begin(), info.username.end(),
              user.user_name.begin());
    std::copy(info.email.begin(), info.email.end(), user.email.begin());

    auto &db_pool = connection_pool<dbng<mysql>>::instance();
    auto conn = db_pool.get();
    if (conn == nullptr) {
      set_server_internel_error(resp);
      return;
    }
    uint64_t id = conn->get_insert_id_after_insert(user);

    if (id == 0) {
      auto err = conn->get_last_error();
      CINATRA_LOG_ERROR << err;
      resp.set_status_and_content(status_type::bad_request, make_error(err));
      return;
    }

    std::string json = make_data(
        user_resp_data{id, info.username, info.email, bool(user.is_verifyed)},
        "注册成功");
    resp.set_status_and_content(status_type::ok, std::move(json));
  }
};
} // namespace purecpp
