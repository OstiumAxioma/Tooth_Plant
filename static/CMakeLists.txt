cmake_minimum_required(VERSION 3.14)
project(CreateComponent LANGUAGES CXX)

# 生成编译数据库供IDE使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 输出静态库到 build/lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# VTK 配置，使用指定的绝对路径
set(VTK_ROOT "D:/Compile/VTK-8.2.0")
set(VTK_DIR "")

# 尝试在指定根目录下常见的安装位置查找 VTKConfig.cmake
set(_vtk_candidates
    "${VTK_ROOT}/lib/cmake/vtk-8.2"
    "${VTK_ROOT}/lib/cmake/vtk-8.2.0"
    "${VTK_ROOT}/build/lib/cmake/vtk-8.2"
    "${VTK_ROOT}/build/lib/cmake/vtk-8.2.0"
)

foreach(_vtk_path ${_vtk_candidates})
    if(EXISTS "${_vtk_path}/VTKConfig.cmake")
        set(VTK_DIR "${_vtk_path}")
        break()
    endif()
endforeach()

if(NOT VTK_DIR)
    message(FATAL_ERROR "VTK 8.2.0 not found under ${VTK_ROOT}. Please verify the installation path.")
endif()

find_package(VTK 8.2 REQUIRED)

# 源文件
set(SOURCES
    src/CreateComponent.cpp
)

# 头文件
set(HEADERS
    header/CreateComponent.h
)

# 静态库目标
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/header
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${VTK_LIBRARIES}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
