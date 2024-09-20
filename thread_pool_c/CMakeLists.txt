cmake_minimum_required(VERSION 3.0.0)
project(threadpool)

add_executable(threadpool threadpool.c main.c)
# 如果使用了多线程，链接pthread库
find_package(Threads REQUIRED)
target_link_libraries(threadpool PRIVATE Threads::Threads)