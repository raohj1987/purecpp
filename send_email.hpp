#pragma once

#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
namespace purecpp {

// 错误处理函数
void error(const std::string& msg) {
  perror(msg.c_str());
  exit(1);
}

// 初始化OpenSSL
void init_openssl() {
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
}

// 创建SSL上下文
SSL_CTX* create_ssl_context(bool is_smtps) {
  const SSL_METHOD* method;
  if (is_smtps) {
    method = SSLv23_client_method();  // SMTPS（SSL/TLS直接连接）
  }
  else {
    method = TLS_client_method();  // STARTTLS（后续升级连接）
  }
  SSL_CTX* ctx = SSL_CTX_new(method);
  if (!ctx) {
    ERR_print_errors_fp(stderr);
    exit(1);
  }
  return ctx;
}

// 建立TCP连接
int create_tcp_socket(const std::string& host, int port) {
  int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("socket creation failed");

  struct hostent* server = ::gethostbyname(host.c_str());
  if (!server)
    error("no such host");

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(port);

  if (::connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    ::close(sockfd);
    error("connection failed");
  }
  return sockfd;
}

// 发送SMTP命令并接收响应
std::string send_smtp_command(int sockfd, ::SSL* ssl, const std::string& cmd,
                              bool use_ssl) {
  std::string send_cmd = cmd + "\r\n";
  ssize_t bytes_sent;
  if (use_ssl) {
    bytes_sent = SSL_write(ssl, send_cmd.c_str(), send_cmd.length());
  }
  else {
    bytes_sent = write(sockfd, send_cmd.c_str(), send_cmd.length());
  }
  if (bytes_sent < 0)
    error("command send failed");

  char buffer[4096];
  memset(buffer, 0, sizeof(buffer));
  ssize_t bytes_read;
  if (use_ssl) {
    bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);
  }
  else {
    bytes_read = read(sockfd, buffer, sizeof(buffer) - 1);
  }
  if (bytes_read < 0)
    error("response read failed");

  return std::string(buffer);
}

// Base64编码（简化实现，仅用于用户名/密码编码）
std::string base64_encode(const std::string& in) {
  static const std::string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  int val = 0, valb = -6;
  for (unsigned char c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(base64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6)
    out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
  while (out.size() % 4) out.push_back('=');
  return out;
}

// 发送邮件核心函数
bool send_email(const std::string& smtp_host, int smtp_port,
                bool is_smtps,  // true: SMTPS(465), false: STARTTLS(587)
                const std::string& username,
                const std::string& password,  // 授权码
                const std::string& from, const std::string& to,
                const std::string& subject, const std::string& body,
                bool is_html = false) {
  // 初始化OpenSSL
  init_openssl();
  SSL_CTX* ctx = create_ssl_context(is_smtps);
  int sockfd = -1;
  ::SSL* ssl = nullptr;
  bool use_ssl = is_smtps;
  bool success = false;

  try {
    // 建立TCP连接
    sockfd = create_tcp_socket(smtp_host, smtp_port);
    std::cout << "TCP connection established\n";

    // SMTPS直接初始化SSL连接，STARTTLS后续升级
    if (is_smtps) {
      ssl = SSL_new(ctx);
      SSL_set_fd(ssl, sockfd);
      if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("SSL connection failed");
      }
      std::cout << "SSL connection established (SMTPS)\n";
    }

    // 1. 接收服务器就绪响应
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = use_ssl ? ::SSL_read(ssl, buffer, sizeof(buffer) - 1)
                                 : ::read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0 || buffer[0] != '2')
      throw std::runtime_error("server not ready: " + std::string(buffer));
    std::cout << "Server response: " << buffer;

    // 2. 发送EHLO命令
    std::string response =
        send_smtp_command(sockfd, ssl, "EHLO localhost", use_ssl);
    if (response[0] != '2')
      throw std::runtime_error("EHLO failed: " + response);
    std::cout << "EHLO response: " << response;

    // 3. STARTTLS（若启用）
    if (!is_smtps) {
      response = send_smtp_command(sockfd, ssl, "STARTTLS", use_ssl);
      if (response[0] != '2')
        throw std::runtime_error("STARTTLS failed: " + response);
      std::cout << "STARTTLS response: " << response;

      // 升级为SSL连接
      ssl = SSL_new(ctx);
      SSL_set_fd(ssl, sockfd);
      if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("SSL upgrade failed");
      }
      use_ssl = true;
      std::cout << "SSL connection upgraded (STARTTLS)\n";

      // 重新发送EHLO
      response = send_smtp_command(sockfd, ssl, "EHLO localhost", use_ssl);
      if (response[0] != '2')
        throw std::runtime_error("EHLO after STARTTLS failed: " + response);
    }

    // 4. 认证（AUTH LOGIN）
    response = send_smtp_command(sockfd, ssl, "AUTH LOGIN", use_ssl);
    if (response[0] != '3')
      throw std::runtime_error("AUTH LOGIN failed: " + response);
    response = send_smtp_command(sockfd, ssl, base64_encode(username), use_ssl);
    if (response[0] != '3')
      throw std::runtime_error("username auth failed: " + response);
    response = send_smtp_command(sockfd, ssl, base64_encode(password), use_ssl);
    if (response[0] != '2')
      throw std::runtime_error("password auth failed: " + response);
    std::cout << "Authentication success\n";

    // 5. 指定发件人
    response =
        send_smtp_command(sockfd, ssl, "MAIL FROM:<" + from + ">", use_ssl);
    if (response[0] != '2')
      throw std::runtime_error("MAIL FROM failed: " + response);

    // 6. 指定收件人
    response = send_smtp_command(sockfd, ssl, "RCPT TO:<" + to + ">", use_ssl);
    if (response[0] != '2')
      throw std::runtime_error("RCPT TO failed: " + response);

    // 7. 发送邮件内容
    response = send_smtp_command(sockfd, ssl, "DATA", use_ssl);
    if (response[0] != '3')
      throw std::runtime_error("DATA failed: " + response);

    std::string email_content = "From: " + from +
                                "\r\n"
                                "To: " +
                                to +
                                "\r\n"
                                "Subject: " +
                                subject;

    // 添加HTML内容类型头部
    if (is_html) {
      email_content += "\r\nContent-Type: text/html; charset=utf-8";
    }

    email_content += "\r\n\r\n" + body + "\r\n.";
    response = send_smtp_command(sockfd, ssl, email_content, use_ssl);
    if (response[0] != '2')
      throw std::runtime_error("DATA content failed: " + response);
    std::cout << "Email content sent\n";

    // 8. 关闭连接
    try {
      response = send_smtp_command(sockfd, ssl, "QUIT", use_ssl);
      // 不严格检查QUIT响应，因为有些服务器可能会立即关闭连接
    } catch (...) {
      // 忽略QUIT命令的任何错误
    }
    success = true;
    std::cout << "Email sent successfully!\n";
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  // 清理资源
  if (ssl)
    SSL_shutdown(ssl);
  if (ssl)
    SSL_free(ssl);
  if (sockfd >= 0)
    ::close(sockfd);
  SSL_CTX_free(ctx);
  EVP_cleanup();

  return success;
}
}  // namespace purecpp
