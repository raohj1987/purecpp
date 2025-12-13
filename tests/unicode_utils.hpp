#pragma once
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace purecpp {

// 将单个Unicode码点转换为UTF-8字符序列
std::string unicode_to_utf8(uint32_t code_point) {
  std::string result;

  if (code_point <= 0x7F) {
    // 1字节UTF-8: 0xxxxxxx
    result.push_back(static_cast<char>(code_point));
  }
  else if (code_point <= 0x7FF) {
    // 2字节UTF-8: 110xxxxx 10xxxxxx
    result.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
    result.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  }
  else if (code_point <= 0xFFFF) {
    // 3字节UTF-8: 1110xxxx 10xxxxxx 10xxxxxx
    result.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
    result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  }
  else if (code_point <= 0x10FFFF) {
    // 4字节UTF-8: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    result.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
    result.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  }
  else {
    // 无效的Unicode码点
    throw std::invalid_argument("Invalid Unicode code point");
  }

  return result;
}

// 将包含\uXXXX或\uXXXXXXXX转义序列的字符串转换为UTF-8字符串
std::string escape_unicode_to_utf8(const std::string& input) {
  std::string result;
  size_t pos = 0;
  const size_t len = input.length();

  while (pos < len) {
    // 查找下一个\u转义序列
    size_t escape_pos = input.find("\\u", pos);

    if (escape_pos == std::string::npos) {
      // 没有更多转义序列，添加剩余部分
      result.append(input.substr(pos));
      break;
    }

    // 添加转义序列前的部分
    result.append(input.substr(pos, escape_pos - pos));

    // 检查是否有足够的字符来解析\uXXXX
    if (escape_pos + 5 > len) {
      // 转义序列不完整，按原样添加
      result.append(input.substr(escape_pos));
      break;
    }

    // 提取4位十六进制数
    std::string hex_str = input.substr(escape_pos + 2, 4);

    try {
      // 转换为整数
      uint32_t code_point = std::stoul(hex_str, nullptr, 16);

      // 转换为UTF-8并添加到结果
      result.append(unicode_to_utf8(code_point));

      // 移动到下一个字符
      pos = escape_pos + 6;
    } catch (const std::exception&) {
      // 转换失败，按原样添加转义序列
      result.append("\\u").append(hex_str);
      pos = escape_pos + 6;
    }
  }

  return result;
}

// 将UTF-8字符串转换为包含\uXXXX转义序列的字符串
std::string utf8_to_escape_unicode(const std::string& input) {
  std::ostringstream result;
  size_t pos = 0;
  const size_t len = input.length();

  while (pos < len) {
    uint8_t byte = static_cast<uint8_t>(input[pos]);

    if (byte <= 0x7F) {
      // ASCII字符，直接输出
      if (byte == '\\') {
        result << "\\\\";
      }
      else {
        result << static_cast<char>(byte);
      }
      pos++;
    }
    else if ((byte & 0xE0) == 0xC0) {
      // 2字节UTF-8
      if (pos + 1 >= len)
        break;

      uint32_t code_point =
          ((byte & 0x1F) << 6) | (static_cast<uint8_t>(input[pos + 1]) & 0x3F);

      result << "\\u" << std::hex << std::setw(4) << std::setfill('0')
             << code_point;
      pos += 2;
    }
    else if ((byte & 0xF0) == 0xE0) {
      // 3字节UTF-8
      if (pos + 2 >= len)
        break;

      uint32_t code_point =
          ((byte & 0x0F) << 12) |
          ((static_cast<uint8_t>(input[pos + 1]) & 0x3F) << 6) |
          (static_cast<uint8_t>(input[pos + 2]) & 0x3F);

      result << "\\u" << std::hex << std::setw(4) << std::setfill('0')
             << code_point;
      pos += 3;
    }
    else if ((byte & 0xF8) == 0xF0) {
      // 4字节UTF-8
      if (pos + 3 >= len)
        break;

      uint32_t code_point =
          ((byte & 0x07) << 18) |
          ((static_cast<uint8_t>(input[pos + 1]) & 0x3F) << 12) |
          ((static_cast<uint8_t>(input[pos + 2]) & 0x3F) << 6) |
          (static_cast<uint8_t>(input[pos + 3]) & 0x3F);

      // 转换为UTF-16代理对
      code_point -= 0x10000;
      uint16_t high_surrogate = 0xD800 | ((code_point >> 10) & 0x3FF);
      uint16_t low_surrogate = 0xDC00 | (code_point & 0x3FF);

      result << "\\u" << std::hex << std::setw(4) << std::setfill('0')
             << high_surrogate;
      result << "\\u" << std::hex << std::setw(4) << std::setfill('0')
             << low_surrogate;
      pos += 4;
    }
    else {
      // 无效的UTF-8序列
      pos++;
    }
  }

  return result.str();
}

}  // namespace purecpp
