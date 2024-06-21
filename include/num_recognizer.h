#ifndef NUM_RECOGNIZER_H
#define NUM_RECOGNIZER_H

#include "num_recognizer_export.h"

// 由于要导出给用c写的cli使用，c++可能会对函数名进行修饰(为了支持重载等)，使用extern "C"，防止函数名被修饰，以便与C代码互相操作
#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明，只声明即可。这里的目的是C和C++代码都能include这个头文件，且对C来说include后可以直接Recognizer *recognizer
而不用struct Recognizer *recognizer。具体是这样的：
声明之后，定义之前，类型会是不完全类型(incomplete type)，不能创建该类型的对象，不能读内部的属性(因为不知道有什么属性)，但是，可以定义
指向该类型的指针和引用(因为指针和引用本质都是指针变量，可以确定大小，所以虽然类型不完全，但是可以创建不完全类型的指针和引用)。
这里typedef的目的是，如果是C，那么是需要在结构体名前面写struct的(例如struct Recognizer recognizer)，这里进行前向声明，
虽然类型不完全，甚至_Recognizer在整个项目中都没有定义，但是由于以下只需要用到Recognizer的指针，即便类型不完全，**编译器只需要知道
Recognizer是个struct的别名就够了**。
后面不管是C还是C++ include这个头文件后都是一致的，以后不用写struct Recognizer **out_recognizer，直接Recognizer **out_recognizer就行。

对于将num_recognizer导出为动态库使用struct Recognizer声明，如果使用typedef struct _Recognizer Recognizer声明，
由于num_recognizer.cpp中有struct Recognizer的定义，会报错Recoginzer重定义。
对于recognize.c要引用num_recognizer.cpp做出的动态库，使用typedef struct _Recognizer Recognizer声明，这样recognize.c
include这个头文件之后可以直接Recognizer *recognizer而不用struct Recognizer *recognizer。struct recognizer的具体定义在
动态库中，链接时会正确解析。但是，同一个.c或者.cpp中，不能
typedef struct _Recognizer Recognizer;
struct Recognizer {
    int x;
};
会提示using typedef-name 'Recognizer' after 'struct'
*/
#ifdef num_recognizer_EXPORTS
struct Recognizer;
#else
typedef struct _Recognizer Recognizer;
#endif

// 初始化手写数字识别库
NUM_RECOGNIZER_EXPORT void num_recognizer_init();

// 创建recognizer，model_path为模型文件路径，out_recognizer为识别器指针的指针，用这个让传进来的指针指向这个函数创建好的一个recognizer
NUM_RECOGNIZER_EXPORT void num_recognizer_create(const char *model_path, Recognizer **out_recognizer);

// 析构recognizer
NUM_RECOGNIZER_EXPORT void num_recognizer_delete(Recognizer* out_recognizer);

// 给出图片信息数组，识别图片中的数字，input_image是28 * 28的float数组，0代表白色，1代表黑色，result为结果变量指针
NUM_RECOGNIZER_EXPORT int num_recognizer_recognize(Recognizer* recognizer, float *input_image, int *result);

// 给出PNG，识别图片中的数字，png_path是.png图片路径，result为结果变量指针
NUM_RECOGNIZER_EXPORT int num_recognizer_recognize_png(Recognizer* recognizer, const char *png_path, int* result);

#ifdef __cplusplus
} // extern "C"的结尾括号
#endif
#endif