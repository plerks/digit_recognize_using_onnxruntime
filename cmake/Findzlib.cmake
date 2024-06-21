# 这是一个自定义查找模块

cmake_minimum_required(VERSION 3.28.3)

find_library(zlib_LIBRARY
    NAMES zlib
    HINTS ENV zlib_ROOT
    PATH_SUFFIXES "lib"
)

add_library(zlib::zlib SHARED IMPORTED)

# 项目不直接使用zlib，所以不需要include zlib的头文件
#target_include_directories(zlib::zlib INTERFACE "$ENV{zlib_ROOT}/include")

if(WIN32)
    set_target_properties(zlib::zlib PROPERTIES
        IMPORTED_IMPLIB ${zlib_LIBRARY})
else()
    set_target_properties(zlib::zlib PROPERTIES
        IMPORTED_LOCATION ${zlib_LIBRARY})
endif()