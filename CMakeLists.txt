cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

project(TinyWebServer VERSION 1.0)

# 测试buffer模块
set(BUFFER_TEST "false")
add_definitions(-D _BUFFER_TEST=\"${BUFFER_TEST}\")

# 测试阻塞队列
set(BLOCKQUEUE_TEST "false")
add_definitions(-D _BLOCKQUEUE_TEST=\"${BLOCKQUEUE_TEST}\")

# 测试Log模块
set(LOG_TEST "true")
add_definitions(-D _LOG_TEST=\"${LOG_TEST}\")

include_directories(${PROJECT_SOURCE_DIR}/common)

aux_source_directory(${PROJECT_SOURCE_DIR}/Buffer BUF_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/Log LOG_SRC)

# 二进制文件保存位置
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(${PROJECT_NAME} 
                main.cpp 
                ${BUF_SRC}
                ${LOG_SRC})

target_link_libraries(${PROJECT_NAME} pthread)