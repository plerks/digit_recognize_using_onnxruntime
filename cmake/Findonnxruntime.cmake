# 这是一个自定义查找模块

cmake_minimum_required(VERSION 3.28.3)

# 查找头文件和库文件位置

find_path(onnxruntime_INCLUDE_DIR onnxruntime_c_api.h
    HINTS ENV onnxruntime_ROOT
    PATH_SUFFIXES "include"
)

find_library(onnxruntime_LIBRARY
    NAMES onnxruntime
    HINTS ENV onnxruntime_ROOT
    PATH_SUFFIXES "lib"
)

# onnxruntime的版本号读入onnxruntime_VERSION中

find_file(onnxruntime_VERSION_FILE VERSION_NUMBER
  HINTS ENV onnxruntime_ROOT)

if(onnxruntime_VERSION_FILE)
  file(STRINGS ${onnxruntime_VERSION_FILE} onnxruntime_VERSION LIMIT_COUNT 1)
endif()

# FindPackageHandleStandardArgs是CMake预置的功能模块，不是查找模块。用FindPackageHandleStandardArgs来判断查找结果变量是否已经赋值有效

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(onnxruntime
    REQUIRED_VARS onnxruntime_INCLUDE_DIR onnxruntime_LIBRARY
    VERSION_VAR onnxruntime_VERSION
    HANDLE_VERSION_RANGE
)

# find_package_handle_standard_args检查完后会设置<软件包名>_FOUND的值
if(onnxruntime_FOUND)
    # 添加库导入目标onnxruntime::onnxruntime，目标名字里可以有::，类似命名空间
    add_library(onnxruntime::onnxruntime SHARED IMPORTED)
    # 注意这里是INTERFACE，让依赖onnxruntime的目标去include，onnxruntime本身不需要自己的头文件
    target_include_directories(onnxruntime::onnxruntime INTERFACE ${onnxruntime_INCLUDE_DIR})

    if(WIN32) # WIN32是CMake的内置变量，用于判断是windows下（64位环境也是用这个变量判断在windows下）
        # windows环境下，导入动态库的IMPORTED_IMPLIB属性要设置为动态库的导入库(.lib而不是.dll)的路径
        set_target_properties(onnxruntime::onnxruntime PROPERTIES
            IMPORTED_IMPLIB ${onnxruntime_LIBRARY})
    else()
        # 其它平台中，导入动态库的IMPORTED_LOCATION属性设置为导入的动态库文件自身的路径。注意：链接时才需要导入库，运行时不需要导入库，要的是dll
        set_target_properties(onnxruntime::onnxruntime PROPERTIES
            IMPORTED_LOCATION "${onnxruntime_LIBRARY}")

        # windows下的动态库(.dll文件)还伴随一个导入库(.lib文件)，而非windows平台动态库只有一个文件(.so或.dylib)，
        # 所以用 IMPORTED_IMPLIB 和 IMPORTED_LOCATION 这两个属性来处理这个差异
    endif()
endif()
