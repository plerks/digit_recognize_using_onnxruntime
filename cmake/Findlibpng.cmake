# 这是一个自定义查找模块

cmake_minimum_required(VERSION 3.28.3)

find_path(libpng_INCLUDE_DIR png.h
    HINTS ENV libpng_ROOT
    PATH_SUFFIXES "include"
)

find_library(libpng_LIBRARY
    NAMES libpng16
    HINTS ENV libpng_ROOT
    PATH_SUFFIXES "lib"
)

add_library(libpng::libpng SHARED IMPORTED) # IMPORTED表明这是个导入库，不是要让cmake去构建

target_include_directories(libpng::libpng INTERFACE ${libpng_INCLUDE_DIR})

if(WIN32)
    set_target_properties(libpng::libpng PROPERTIES
        IMPORTED_IMPLIB ${libpng_LIBRARY})
else()
    set_target_properties(libpng::libpng PROPERTIES
        IMPORTED_LOCATION ${libpng_LIBRARY})
endif()