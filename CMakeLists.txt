cmake_minimum_required(VERSION 3.14)
project(VTK_Qt_Project LANGUAGES CXX)

# 加载本地配置文件（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.local.cmake")
    message(STATUS "Loading local configuration from config.local.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/config.local.cmake")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.cmake")
    message(STATUS "Loading configuration from config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/config.cmake")
endif()

# 生成编译数据库供IDE使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Exe)

# Qt设置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR})

# Qt5配置 - 自动查找（不依赖环境变量）
if(NOT DEFINED Qt5_DIR)
    # 尝试常见的Qt安装路径（不依赖环境变量）
    set(Qt5_PATHS
        "C:/Qt/Qt5.14.2/5.14.2/msvc2017_64"
        "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64"
        "C:/Qt/5.14.2/msvc2017_64"
        "C:/Qt/5.12.9/msvc2017_64"
        "D:/Qt/Qt5.14.2/5.14.2/msvc2017_64"
        "D:/Qt/Qt5.12.9/5.12.9/msvc2017_64"
    )

    foreach(path ${Qt5_PATHS})
        if(EXISTS "${path}/lib/cmake/Qt5")
            set(Qt5_DIR "${path}/lib/cmake/Qt5")
            set(CMAKE_PREFIX_PATH "${path}")
            message(STATUS "Found Qt5 at: ${Qt5_DIR}")
            break()
        endif()
    endforeach()
endif()

# 如果还是找不到Qt5，提示用户设置
if(NOT DEFINED Qt5_DIR)
    message(WARNING "Qt5 not found automatically. Please set Qt5_DIR or CMAKE_PREFIX_PATH")
    message(WARNING "You can set it by: cmake -DQt5_DIR=<path_to_qt5>/lib/cmake/Qt5")
endif()

# 查找Qt包
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

# VTK配置 - 使用指定的绝对路径
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

# VTK Qt支持检测和配置
if(VTK_QT_FOUND)
    message(STATUS "VTK Qt支持已找到")
else()
    set(VTK_QT_FOUND TRUE)
    message(STATUS "VTK Qt支持已启用")
endif()

# UI文件设置
set(UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/header)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR})

# 源文件
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
)

# 头文件
set(HEADERS
    header/mainwindow.h
)

# UI文件
set(FORMS
    src/mainwindow.ui
)

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FORMS})

# 添加包含目录
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/header
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/static/header  # 添加静态库头文件目录
)

# 设置编译定义
target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_DEPRECATED_WARNINGS
)

# 设置MSVC编译器UTF-8支持
if(MSVC)
    # 设置源文件字符集为UTF-8，解决中文字符编译警告
    add_compile_options("/source-charset:utf-8")
    # 同时设置执行字符集为UTF-8
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# 查找静态库文件
find_library(CREATE_COMPONENT_LIB_RELEASE 
    NAMES CreateComponent
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/static/build/lib/Release
    NO_DEFAULT_PATH
)

find_library(CREATE_COMPONENT_LIB_DEBUG
    NAMES CreateComponent_d CreateComponent
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/static/build/lib/Debug
    NO_DEFAULT_PATH
)

# 检查是否找到静态库
if(NOT CREATE_COMPONENT_LIB_RELEASE)
    message(WARNING "CreateComponent.lib not found in ${CMAKE_CURRENT_SOURCE_DIR}/lib or static/build/lib/Release")
endif()

if(NOT CREATE_COMPONENT_LIB_DEBUG)
    message(WARNING "CreateComponent_d.lib not found in ${CMAKE_CURRENT_SOURCE_DIR}/lib or static/build/lib/Debug")
endif()

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    ${VTK_LIBRARIES}
)

# 根据配置链接对应的静态库
if(CREATE_COMPONENT_LIB_RELEASE OR CREATE_COMPONENT_LIB_DEBUG)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:${CREATE_COMPONENT_LIB_DEBUG}>
        $<$<CONFIG:Release>:${CREATE_COMPONENT_LIB_RELEASE}>
    )
else()
    message(FATAL_ERROR "Static library CreateComponent not found! Please build static library (cd static && build.bat) or copy libs into lib/.")
endif()

# 部署Qt DLL
if(WIN32)
    # 找到windeployqt.exe
    get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    
    # 添加命令在构建后运行windeployqt
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --verbose 0
            --no-translations
            --no-compiler-runtime
            --no-opengl-sw
            "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "正在部署Qt..."
    )
endif()
