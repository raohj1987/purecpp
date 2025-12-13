#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../entity.hpp"
#include "../send_email.hpp"
#include "../thirdparty/doctest/doctest.h"
#include "../user_password.hpp"

// 主函数测试
TEST_CASE("Send Email Tests") {
  auto option_conf = purecpp::load_smtp_config();
  CHECK(option_conf);
  if (option_conf) {
    std::cerr << "无法加载SMTP配置" << std::endl;
    return;
  }
  auto conf = option_conf.value();
  // 配置参数（需替换为实际信息）
  std::string smtp_host = conf.smtp_host;
  int smtp_port = conf.smtp_port;  // QQ邮箱SMTPS端口465，STARTTLS端口587
  bool is_smtps = true;
  std::string username = conf.smtp_user;  // 发件人邮箱
  std::string password = conf.smtp_password;  // 邮箱授权码（非密码）
  std::string from = username;
  std::string to = "your_email@163.com";  // 收件人邮箱
  std::string subject = "Test Email (SMTP Protocol)";
  std::string body =
      "Hello, this is a test email sent via manual SMTP implementation in "
      "C++!\n";

  // 发送邮件
  bool result = purecpp::send_email(smtp_host, smtp_port, is_smtps, username,
                                    password, from, to, subject, body);
  CHECK(result == true);
}

// 测试发送HTML格式邮件
TEST_CASE("Send HTML Email Tests") {
  auto option_conf = purecpp::load_smtp_config();
  CHECK(option_conf);
  if (option_conf) {
    std::cerr << "无法加载SMTP配置" << std::endl;
    return;
  }
  auto conf = option_conf.value();
  std::string smtp_host = conf.smtp_host;
  int smtp_port = conf.smtp_port;  // QQ邮箱SMTPS端口465，STARTTLS端口587
  bool is_smtps = true;
  std::string username = conf.smtp_user;  // 发件人邮箱
  std::string password = conf.smtp_password;  // 邮箱授权码（非密码）
  std::string from = username;
  std::string to = "your_email@163.com";  // 收件人邮箱
  std::string subject = "Test HTML Email (SMTP Protocol)";
  
  // HTML格式的邮件正文
  std::string html_body = 
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "    <title>Test HTML Email</title>\n"
      "</head>\n"
      "<body>\n"
      "    <h1 style='color: blue;'>Hello World!</h1>\n"
      "    <p>This is a <strong>HTML format</strong> test email sent via manual SMTP implementation in C++.</p>\n"
      "    <p>Here is a list:</p>\n"
      "    <ul>\n"
      "        <li>Item 1</li>\n"
      "        <li>Item 2</li>\n"
      "        <li>Item 3</li>\n"
      "    </ul>\n"
      "    <p>Regards,<br>SMTP Test Team</p>\n"
      "</body>\n"
      "</html>";

  // 发送HTML格式邮件，设置is_html为true
  bool result = purecpp::send_email(smtp_host, smtp_port, is_smtps, username,
                                    password, from, to, subject, html_body, true);
  CHECK(result == true);
}

// 这个测试用例使用了不相关的库和命名空间，暂时注释掉
/*
TEST_CASE("Send Reset Email Tests") {
  smtp::email_server svr_conf{
      "smtp.qq.com",      // smtp 邮箱服务器地址
      "465",               // smtp 邮箱服务器端口
      "xxxxxx@qq.com",  // 你的邮箱用户名
      "xxxxxxxxxxxxxxxx"   // 邮箱授权码，不是密码，需要登录邮箱去设置
  };

  smtp::email_data data{
      "xxxxxx@qq.com",     // 发件人邮箱
      {"xxxxxx@163.com"},  // 收件人邮箱列表
      "test",                 // 邮件标题
      "it is a text"          // 邮件正文
  };

  auto client = smtp::get_smtp_client<cinatra::SSL>(
      coro_io::get_global_executor()->get_asio_executor().context());
  client.set_email_server(svr_conf);
  client.set_email_data(data);
  client.start();
}
*/
