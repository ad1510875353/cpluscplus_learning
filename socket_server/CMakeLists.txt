cmake_minimum_required(VERSION 3.0.0)
project(server VERSION 0.1.0 LANGUAGES C CXX)

add_executable(server TaskQueue.hpp ThreadPool.hpp server.cpp)

# 如果使用了多线程，链接pthread库
find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)