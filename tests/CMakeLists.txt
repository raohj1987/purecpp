# tests/CMakeLists.txt

# 启用测试
enable_testing()

# 添加登录测试
add_executable(test_login test_login.cpp)
target_link_libraries(test_login ormpp)
add_test(NAME test_login COMMAND test_login)


# 添加JWT token测试
add_executable(test_jwt_token test_jwt_token.cpp)
target_link_libraries(test_jwt_token ormpp)
add_test(NAME test_jwt_token COMMAND test_jwt_token)

# 添加发送邮件测试
find_package(OpenSSL REQUIRED)
add_executable(test_send_email test_sendemail.cpp)
target_compile_options(test_send_email PRIVATE -DCINATRA_ENABLE_SSL)
target_link_libraries(test_send_email ormpp OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_send_email COMMAND test_send_email)

# 添加Unicode工具测试
add_executable(test_unicode_utils test_unicode_utils.cpp)
target_link_libraries(test_unicode_utils ormpp)
add_test(NAME test_unicode_utils COMMAND test_unicode_utils)
