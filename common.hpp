#pragma once
#include <chrono>
#include <string_view>

#include "entity.hpp"
#include <cinatra.hpp>
#include <iguana/json_reader.hpp>
#include <iguana/json_writer.hpp>

using namespace cinatra;

namespace purecpp {
inline std::string make_error(std::string_view err_msg) {
  rest_response<std::string_view> data{false, std::string(err_msg)};
  std::string json;
  iguana::to_json(data, json);
  return json;
}

template <typename T> inline std::string make_data(T t, std::string msg = "") {
  rest_response<T> data{};
  data.success = true;
  data.message = std::move(msg);
  data.data = std::move(t);

  std::string json;
  try {
    iguana::to_json(data, json);
  } catch (std::exception &e) {
    json = "";
    CINATRA_LOG_ERROR << e.what();
  }

  return json;
}

inline void set_server_internel_error(auto &resp) {
  resp.set_status_and_content(
      status_type::internal_server_error,
      make_error(to_http_status_string(status_type::internal_server_error)));
}

inline uint64_t get_timestamp_milliseconds() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  return static_cast<uint64_t>(milliseconds.count());
}
} // namespace purecpp
