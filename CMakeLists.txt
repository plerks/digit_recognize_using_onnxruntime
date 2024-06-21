cmake_minimum_required(VERSION 3.28.3)

project(digit-recognize-using-onnxruntime)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}") # CMAKE_CURRENT_SOURCE_DIR是当前处理的CMakeLists.txt所在的路径
set(CMAKE_CXX_STANDARD 11) # 设置C++标准为11
set(onnx_version 1.18.0) # onnxruntime的版本

set(CMAKE_ENCODING_UTF8 TRUE) # 让CMake输出的信息使用UTF-8编码

# 设置源码字符集和执行字符集为UTF-8，项目所有源文件都是UTF-8的，这里主要的问题是如果用Visual Studio运行，VS默认文件是GB2312编码，
# 解析源码会直接报错。试了下把源文件编码改成UTF-8 with BOM，VS能正常编译，应该是编译器会探测头部然后处理接下来的正文，但是
# UTF-8 with BOM原理是在文件开头加几个魔数表明文件是UTF-8编码，影响了文件内容了。
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # 对gcc
  add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()
if(MSVC) # 对MSVC。这里/utf-8是把源字符集和执行字符集都设置为UTF-8，见<https://learn.microsoft.com/zh-cn/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8?view=msvc-170>
  add_compile_options(/utf-8)
endif()

# onnxruntime的库所放位置。我用的是<https://github.com/microsoft/onnxruntime/releases/tag/v1.18.0> onnxruntime-win-x64-1.18.0.zip中的文件
set(ENV{onnxruntime_ROOT} "third-party/onnxruntime")
set(ENV{libpng_ROOT} "third-party/libpng")
set(ENV{zlib_ROOT} "third-party/zlib")

# find_package会调用Find<软件包名>.cmake的查找模块来查找软件包。会去CMAKE_MODULE_PATH变量定义的路径列表中查找模块。
# 这样自定义的查找模块就能得到加载，find_package查找成功后会有一个<软件包名>_FOUND的变量指示查找结果。
find_package(onnxruntime 1.18.0...<2.0.0 REQUIRED)
find_package(zlib REQUIRED) # find之后会有一个库目标叫做zlib::zlib
find_package(libpng REQUIRED)

# 把num_recognizer做成动态库
add_library(num_recognizer SHARED src/num_recognizer.cpp)

# 这里generate_export_header(num_recognizer)会生成一个num_recognizer_export.h头文件，这个头文件作用单纯是定义宏。
# 对于MSVC，生成和使用动态库时需要用__declspec(dllimport)和__declspec(dllimport)标记导入导出的内容。
# num_recognizer_export.h会根据有无num_recognizer_EXPORTS定义，将NUM_RECOGNIZER_EXPORT定义为
# __declspec(dllimport)和__declspec(dllimport)，这样num_recognizer.h中就可以直接用NUM_RECOGNIZER_EXPORT
# 这个宏来标记内容，生成动态库时，需要定义一个num_recognizer_EXPORTS宏，然后NUM_RECOGNIZER_EXPORT展开为__declspec(dllexport)
# 使用动态库时，由于没有定义num_recognizer_EXPORTS宏，NUM_RECOGNIZER_EXPORT展开为__declspec(dllimport)，
# 相当于NUM_RECOGNIZER_EXPORT智能变成__declspec(dllimport)和__declspec(dllimport)。
# 
# 参考:
# <https://www.cnblogs.com/fortunely/p/16297277.html>
# <https://blog.csdn.net/yaked/article/details/109025157>
# <https://learn.microsoft.com/zh-cn/cpp/build/walkthrough-creating-and-using-a-dynamic-link-library-cpp?view=msvc-170>
# <https://learn.microsoft.com/zh-cn/cpp/build/exporting-from-a-dll-using-declspec-dllexport?view=msvc-170>
# <https://learn.microsoft.com/zh-cn/cpp/build/importing-function-calls-using-declspec-dllimport?view=msvc-170>
# <https://blog.csdn.net/huangyimo/article/details/81748939>
include(GenerateExportHeader)
generate_export_header(num_recognizer)

set_target_properties(num_recognizer PROPERTIES
  CXX_VISIBILITY_PRESET hidden # 若未显式标记导出，则不导出，减少导出的符号数量
  VISIBILITY_INLINES_HIDDEN 1 # 隐藏内联函数的符号
)

# 需要将${CMAKE_BINARY_DIR}加入num_recognizer这个库目标的头文件搜索目录，以让num_recognizer可以使用num_recognizer_export.h
target_include_directories(num_recognizer PUBLIC include ${CMAKE_BINARY_DIR})

target_link_libraries(num_recognizer PRIVATE onnxruntime::onnxruntime libpng::libpng)

# 注意，这里为num_recognizer这个库目标定义了num_recognizer_EXPORTS这个宏
target_compile_definitions(num_recognizer PRIVATE ORT_NO_EXCEPTIONS num_recognizer_EXPORTS)

add_executable(recognize cli/recognize.c)
target_link_libraries(recognize PRIVATE num_recognizer) # 这里可执行文件目标recognize依赖于库目标num_recognizer，构建recognize时也会include ${CMAKE_BINARY_DIR}，能找到num_recognizer_export.h

if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "") # 注意条件语法中变量名不要加${}，不过写成if("${CMAKE_BUILD_TYPE}" STREQUAL "")也行
  set(BUILD_TYPE "Debug")
else()
  set(BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()

if (WIN32)
  # 将onnxruntime.dll, libpng16.dll, zlib.dll复制到recognize.exe同一目录中，Windows下会在可执行文件所在目录查找dll，所以能查找到。
  # 注意运行时不需要导入库，只把dll复制过去即可，链接时才需要导入库。
  # 也就是说，这部分的代码的目的是把dll复制过去，有意思的是这样做既不能达到目的也能达到目的，具体见README.md
  find_path(recognize_DIR recognize.exe
    HINTS "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/${BUILD_TYPE}"
  )
  # 用find_path()找到recognize.exe所在目录，保存在变量recognize_DIR中，如果没找到，recognize_DIR的值会是"recognize_DIR-NOTFOUND"
  if (NOT recognize_DIR STREQUAL "recognize_DIR-NOTFOUND")
    file(COPY third-party/onnxruntime/lib/onnxruntime.dll DESTINATION "${recognize_DIR}")
    file(COPY third-party/libpng/lib/libpng16.dll DESTINATION "${recognize_DIR}")
    file(COPY third-party/zlib/lib/zlib.dll DESTINATION "${recognize_DIR}")
  endif()
endif()