cmake_minimum_required(VERSION 3.10)
project(webserver)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 添加编译选项,多线程要求
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

file(GLOB 
    SOURCES   
    "code/log/*.cpp" 
    "code/log/*.h" 
    "code/http/*.cpp" 
    "code/buffer/*.cpp" 
    "code/server/*.cpp"
    "code/server/*.h"
    "code/timer/*.cpp"
    "code/pool/*.cpp" 
    "code/pool/*.h" 
    "main.cpp"
)

# 添加可执行文件
add_executable(webserver ${SOURCES})
target_link_libraries(webserver mysqlclient)
