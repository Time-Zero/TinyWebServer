cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(TinyWebServer VERSION 1.0)

# 测试buffer模块
set(BUFFER_TEST "true")
add_definitions(-D _BUFFER_TEST=\"${BUFFER_TEST}\")


aux_source_directory(${PROJECT_SOURCE_DIR}/Buffer BUF_SRC)

# 二进制文件保存位置
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(${PROJECT_NAME} 
                main.cpp 
                ${BUF_SRC})