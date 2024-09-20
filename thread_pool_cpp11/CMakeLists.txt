cmake_minimum_required(VERSION 3.0.0)
project(threadpool VERSION 0.1.0 LANGUAGES C CXX)


# # 设置默认的编译器优化标志为-O0（GCC和Clang）或/Od（MSVC）  
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")  
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")  
# elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")  
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Od")  
# endif() 

add_executable(threadpool TaskQueue.hpp ThreadPool.hpp main.cpp)

# 如果使用了多线程，链接pthread库
find_package(Threads REQUIRED)
target_link_libraries(threadpool PRIVATE Threads::Threads)