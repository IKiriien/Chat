cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost 1.70 COMPONENTS system thread REQUIRED)

add_executable(Server ../Common/message.cpp Server.cpp)

target_compile_features(Server PUBLIC cxx_std_17)
target_compile_options(Server PRIVATE -Wall -Wextra -pedantic -Werror)

target_include_directories(Server PUBLIC Headers "../Common")
target_include_directories(Server PUBLIC Headers ${OPENSSL_INCLUDE_DIR})
target_include_directories(Server PUBLIC Headers ${Boost_INCLUDE_DIR})

target_link_libraries(Server ${OPENSSL_LIBRARIES})
target_link_libraries(Server ${Boost_LIBRARIES})

