本仓库的内容是阅读《CMake构建实战：项目开发卷》(ISBN 978-7-115-61664-7)后，实践其最后一章的一个练手项目。

项目内容是，使用onnxruntime库实现图片中手写数字的识别，项目的主要目的是CMake的构建，因此这里并不关心onnxruntime的使用和具体的数字识别原理。

## 运行环境
原书中源代码有平台区别的部分作了条件编译，应该是能跨平台编译的，不过我没试。主要的问题在于，我为了能让项目自包含，所以把onnxruntime，libpng和zlib的头文件和库文件都放到了third-party目录下进行引用，而这3个库都是在Windows下，用MSVC作为生成器构建出来的，所以不能在其它环境下使用。因此，

**工程仅在Windows下可用，且需以MSVC为生成器**。

## 相关准备
项目需要用到libpng库处理png图片，而libpng库又依赖zlib库，所以需要先构建这两个库。

构建zlib，[github地址](https://github.com/madler/zlib)，克隆或下载到本地后：
```shell
> cd zlib
> mkdir build
> cd build
> cmake -DCMAKE_BUILD_TYPE=Release ..
> cd --build . --config Release
> cmake --install . # 需要管理员权限
```
构建libpng，[github地址](https://github.com/pnggroup/libpng)，同样的方式：
```shell
> cd libpng
> mkdir build
> cd build
> cmake -DCMAKE_BUILD_TYPE=Release ..
> cmake --build . --config Release
> cmake --install . # 需要管理员权限
```

onnxruntime，直接从[Release](https://github.com/microsoft/onnxruntime/releases/tag/v1.18.0)下载onnxruntime-win-x64-1.18.0.zip。

onnxruntime模型文件，从[onnx仓库](https://github.com/onnx/onnx)下载[Pre-trained ONNX models](https://github.com/onnx/models)，具体下载地址为<https://github.com/onnx/models/tree/main/validated/vision/classification/mnist/model>。

`cmake --install`安装cmake项目时，Windows中默认安装在C:\Program Files (x86)\libpng，不过我这里不需要安装，build完成之后，直接把相关的头文件和库文件放在了工程third-party目录下。

以上这些工程中都已直接准备好。

CMake预置了zlib的查找模块，所以原书中对zlib的查找直接用的内置的zlib查找模块，我是自己写了个。

## 项目逻辑
项目主体是num_recognizer.cpp用onnxruntime库去识别图片中的数字并给出识别结果，如果是直接给出png图片路径，需要使用libpng读取并将png图片转换为28 * 28的二值化图片数组，再用onnxruntime进行识别。对于识别的部分，构建了个动态库num_recognizer。

工程还有个简单的命令行工具，recognize.c调用构建出的动态库num_recognizer中的函数，构建成命令行工具后通过命令行调用识别数字。cli是用c写的，为了让cli能正常调用c++构建出的动态库num_recognizer，num_recognizer在暴露API时，接口必须是c风格的，函数的参数、返回值都必须是c中支持的类型，不能使用异常，且相应代码要放在extern "C"块中。

## 运行
以Debug模式构建为例。

### 命令行运行
在项目根目录下：
```shell
> mkdir build
> cd build
> cmake -DCMAKE_BUILD_TYPE=Debug ..
> cmake --build . --config Debug
```
这样之后在build/Debug目录下就有了动态库num_recognizer和命令行工具recognize.exe。然后，还需要把third-party中的onnxruntime.dll，libpng16.dll，zlib.dll复制到recognize.exe同一目录下，Windows下会在可执行文件所在目录查找dll，所以能查找到。

然后，在项目根目录下运行`./build/Debug/recognize.exe models/mnist-8.onnx 2.png`即可看到识别数字的结果。

**注意：这里有个很有意思的点**，CMakeLists.txt中，我尝试查找recognize.exe所在的目录并用file()命令自动把上述的3个dll复制过去(具体代码见CMakeLists.txt)，但是实际`cmake .. && cmake --build .`之后，并未成功把dll复制过去，但是，这时候如果再执行一次`cmake ..`，dll就能成功被复制过去。

为什么第3条命令再执行一次`cmake ..`，dll就能成功被复制过去？

关键在于CMake构建原理的三个步骤([参考](https://cloud.baidu.com/article/3282227))：

1. 配置阶段。CMake首先读取CMakeLists.txt文件，并根据其中的指令生成一个名为CMakeCache.txt的缓存文件。这个文件保存了项目的配置信息，如编译器选择、编译选项等。
2. 生成阶段。接着，CMake根据CMakeCache.txt文件生成适用于目标平台的构建文件。这些构建文件包含了编译规则、依赖关系等信息，用于指导后续的编译过程。
3. 构建阶段。使用生成的构建文件，调用构建系统来构建项目。

注意，CMakeLists.txt在配置阶段就解析完了，也就是说，CMakeLists.txt中的命令是在配置阶段被执行的，对于上述的file()，其执行时，recognize.exe还没被构建出来！因此自然找不到recognize.exe所在的目录，复制dll也就无法成功。

而运行完了`cmake --build .`之后，构建产物是已经生成了的，recognize.exe是有的，这时重新再执行一次`cmake ..`，其中的file命令就能把dll复制过去了。

这里重新再执行一次`cmake ..`把dll复制过去不是一个优雅的做法，根本原因在于不适合使用在配置阶段运行的file命令实现在构建完成后把dll复制过去。

### 在VSCode中运行
关于怎么使用VSCode运行CMake项目见[这个仓](https://github.com/plerks/cmake-windows-vscode-mingw)。

注意构建完成后，可以选择手动复制dll过去，或者运行`cmake ..`复制dll过去，也可以对着CMakeLists.txt按ctrl + s触发IDE重新运行`cmake ..`

这里recognize.exe需要命令行参数，需要类似这样的setting.json文件：
```json
{
    "cmake.generator": "Visual Studio 17 2022", // 视自己的版本更改，cmake --help会列出当前平台可用的生成器
    "cmake.buildDirectory": "${workspaceFolder}/build", // 构建目录
    "cmake.debugConfig": {
        "args": ["${workspaceFolder}/models/mnist-8.onnx", "${workspaceFolder}/2.png"] // recognize.exe的命令行参数
    }
}
```
### 在VS中运行
直接用VS打开项目，构建完成后对dll的处理相似。

对着CMakeLists.txt右键，然后点添加调试配置，需要类似这样的launch.vs.json文件：
```json
{
  "version": "0.2.1",
  "defaults": {},
  "configurations": [
    {
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "recognize.exe",
      "name": "recognize.exe",
      "args": [ "${workspaceRoot}/models/mnist-8.onnx", "${workspaceRoot}/2.png" ] // 命令行参数
    }
  ]
}
```
对着CMakeLists.txt右键之后也可以点 xxx的CMake设置 或者 CMake概述页，给VS配置相关信息，例如输出目录等。

## 总结
CMake最关键的概念抽象应该是在于"目标"(库目标，可执行文件目标等)这个概念，target_link_libraries()和target_include_directories()的PRIVATE, INTERFACE, PUBLIC属性指明了目标的依赖的作用范围。这样一来，对于一个目标，CMake就可以根据其直接依赖计算间接依赖，从而得到构建产物所需的依赖关系，从而知道该用什么命令构建产物，和Maven等包管理器相似。此外，依赖静态库和依赖动态库应该还不太一样，静态依赖的边是不需要搜索下去的，动态依赖的边需要。

setjmp() longjmp()这两个函数，可以在C中实现类似异常处理的效果，很特别。

实践cli的制作，把功能封装成库，然后写一个命令行工具调用库实现功能。

cpack，书中第6章示范了如何使用cpack便捷地生成安装程序。对于这种较小的程序分发非常方便。

关于powershell不显示错误信息的问题，例如，如果没把3个dll复制过去，那么用git bash运行recognize.exe会提示找不到动态库然后程序退出，但是powershell就什么信息都不会打印，直接退出。