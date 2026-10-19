cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME OS996)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/bsp_gxht30.c
    Core/Src/bsp_pir.c
    Core/Src/bsp_mq2.c
    Core/Src/bsp_alarm.c 
    Core/Src/bsp_protocol.c
    Core/cJSON/cJSON.c
    Core/Src/log.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Core/cJSON
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# ------------------------------------------------------------------
# 启用 newlib-nano 的浮点格式化支持
# ------------------------------------------------------------------
# 为什么需要：
#   newlib-nano 为了省空间，默认【不支持】%f / %lf 等浮点格式，
#   只支持整数。cJSON 在序列化 double 时会调用 sprintf("%1.15g", ...)，
#   不启用就会链接失败：undefined reference to `_printf_float`
#
# 代价：
#   Flash 增加约 6~10 KB（拉入 vfprintf 的浮点分支）
#
# -Wl,-u,_printf_float  的含义：
#   -Wl,    把后面的参数原样传给链接器 ld
#   -u      强制"未定义符号"，即强制把 _printf_float 这个符号
#           拉进最终镜像（即使没有直接引用它也保留）
#
# 注意：开启后 printf 的浮点【仅在需要时】才走浮点分支，
#       整数格式的性能和体积不受影响。
# ------------------------------------------------------------------
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,-u,_printf_float
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
