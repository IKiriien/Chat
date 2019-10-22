cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost 1.70 COMPONENTS system thread REQUIRED)

add_executable(Client ../Common/message.cpp Client.cpp)

target_compile_features(Client PUBLIC cxx_std_17)
target_compile_options(Client PRIVATE -Wall -Wextra -pedantic -Werror)

target_include_directories(Client PUBLIC Headers "../Common")
target_include_directories(Client PUBLIC Headers ${OPENSSL_INCLUDE_DIR})
target_include_directories(Client PUBLIC Headers ${Boost_INCLUDE_DIR})

target_link_libraries(Client ${OPENSSL_LIBRARIES})
target_link_libraries(Client ${Boost_LIBRARIES})

