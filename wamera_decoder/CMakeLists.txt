cmake_minimum_required(VERSION 3.22)

project(WameraDecoder)

# 设置 C 编译标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# TODO 忽略未使用的结果 一会儿删除
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")

# 设置编译输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 设置编译类型
set(CMAKE_BUILD_TYPE "Debug")

# 设置源文件
set(
    SRC_LIST
    src/main.c
    src/utils/logger.c src/utils/tool.c
    src/core/camera.c src/core/codec.c
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SRC_LIST})

# 包含头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 设置编译选项
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Werror
)

# 指定链接的库（如果有）
target_link_libraries(${PROJECT_NAME} PRIVATE
    PkgConfig::ffmpeg
)
