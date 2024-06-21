#include <num_recognizer.h>
#include <cstdint>
#include <array>
#include <onnxruntime_cxx_api.h>
#include <png.h>

#if _WIN32
#include <Windows.h>
#endif

static const char *INPUT_NAMES[] = { "Input3" }; // 模型输入参数名
static const char* OUTPUT_NAMES[] = { "Plus214_Output_0" }; // 模型输出参数名
static constexpr int64_t INPUT_WIDTH = 28; // 模型输入图片宽度
static constexpr int64_t INPUT_HEIGHT = 28;
static const std::array<int64_t, 4> input_shape{ 1, 1, INPUT_WIDTH, INPUT_HEIGHT }; // 输入数据的形状（各维度大小），前两个1分别表示每次推理处理一个样本以及输入数据是单通道的
static const std::array<int64_t, 2> output_shape{ 1, 10 }; // 输出数据的形状（各维度大小）

static Ort::Env env{ nullptr }; // onnxruntime环境
static Ort::MemoryInfo memory_info{ nullptr }; // onnxruntime内存信息

// 识别数字类
struct Recognizer {
    Ort::Session session; // onnxruntime会话
};

// 把像素颜色二值化
static float to_binary_float(png_byte b) {
    return b < 128 ? 1 : 0;
}

// 这个方法不会暴露给外部，不用放在extern "C"里面，放任C++用修饰后的函数名不会有影响
static float get_float_color_in_png(int i, int j, int png_width, int png_height, png_byte color_type, png_bytep* png_data) {
    if (i < 0 || i >= png_height || j < 0 || j >= png_width) return 0;
    png_bytep p = nullptr;
    switch (color_type) {
        case PNG_COLOR_TYPE_RGB: // RGB图片
            p = png_data[i] + j * 3; // RGB图片，每个像素点占3个byte，所以这里 * 3
            return to_binary_float((p[0] + p[1] + p[2]) / 3);
            break;
        case PNG_COLOR_TYPE_RGBA: // RGBA图片
            p = png_data[i] + j * 4; // RGB图片，每个像素点占4个byte，所以这里 * 4
            return to_binary_float((p[0] + p[1] + p[2]) * p[3] / 3);
            break;
        default: return 0;
    }
}

// 由于要导出给用c写的cli使用，c++可能会对函数名进行修饰(为了支持重载等)，使用extern "C"，防止函数名被修饰，以便与C代码互相操作
extern "C" {

void num_recognizer_init() {
    env = Ort::Env{static_cast<const OrtThreadingOptions *>(nullptr)}; // 这里必须加上类型转换，不然调的构造函数会是Env(std::nullptr_t)
    memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
}

void num_recognizer_create(const char* model_path, Recognizer** out_recognizer) {
    Ort::Session session{ nullptr };
    // windows里Ort::Session构造时要用wchar_t*，用MultiByteToWideChar转换成宽字符串
#if _WIN32 // 条件编译
    wchar_t wpath[256];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, model_path, -1, wpath, 256);
    session = Ort::Session(env, wpath, Ort::SessionOptions(nullptr));
#else
    session = Ort::Session(env, model_path, Ort::SessionOptions(nullptr));
#endif
    *out_recognizer = new Recognizer{ std::move(session) };
}

void num_recognizer_delete(Recognizer* recognizer) {
    delete recognizer;
}

int num_recognizer_recognize(Recognizer* recognizer, float* input_image, int* result) {
    std::array<float, 10> results{};

    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_image, INPUT_WIDTH * INPUT_HEIGHT,
        input_shape.data(), input_shape.size()
    );

    auto output_tensor = Ort::Value::CreateTensor<float>(
        memory_info, results.data(), results.size(),
        output_shape.data(), output_shape.size()
    );

    // mnist.onnx接收的输入是28 * 28的float数组作为输入图片，其中0代表白色，1代表黑色，然后分别输出0-10的可能性权重
    recognizer->session.Run(Ort::RunOptions{ nullptr }, INPUT_NAMES, &input_tensor, 1, OUTPUT_NAMES, &output_tensor, 1);

    // 这里std::distance()返回的是最大元素的下标，而使用的模型跑的结果是0到10的可能性权重，所以最大元素的下标就是最大可能的数字
    *result = static_cast<int>(std::distance(results.begin(), std::max_element(results.begin(), results.end())));
    return 0;
}

// 给出png图片的路径，要做的是把png图片用libpng改成28 * 28的float数组，然后用num_recognizer_recognize()判断
int num_recognizer_recognize_png(Recognizer* recognizer, const char* png_path, int* result) {
    std::array<float, INPUT_WIDTH* INPUT_HEIGHT> input_image;
    FILE* fp;
    unsigned char header[8];
    fp = fopen(png_path, "rb");
    if (fp == NULL) {
        return -2;
    }

    // png文件有特定的头，把png的文件头读出来，看看是不是png文件
    fread(header, 1, 8, fp);

    if (png_sig_cmp((unsigned char *)header, 0, 8)) {
        fclose(fp);
        return -3;
    }

    // 创建png_struct指针
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

    if (!png_ptr) {
        fclose(fp);
        return -4;
    }

    // 创建png_info指针
    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
        fclose(fp);
        return -5;
    }

    /* 参考<https://www.cnblogs.com/hazir/p/c_setjmp_longjmp.html>, <http://www.libpng.org/pub/png/libpng-1.2.5-manual.html>
    这里很特别，setjmp()和longjmp()配合使用，能实现类似异常处理的效果，也能实现跨函数范围的goto效果。
    * setjmp(jmp_buf env)会将程序当前的执行状态保存于env中，例如栈指针，指令指针，然后longjmp(jmp_buf env, int val)会跳转
    * 会刚刚要开始执行setjmp的位置，相当于，setjmp()标记返回点，如果后面的执行出现问题，可以像抛出异常一样执行longjmp()，返回之前的
    * 标记点。setjmp()类似于catch，longjmp()类似于throw。
    * 
    * setjmp()的返回值有两种情形:
    * 如果setjmp()直接调用，返回值为0。
    * 如果setjmp()是由于longjmp()返回而被调用的，返回由longjmp()设定的val(通常非0)。这个不知道具体怎么实现的，不过至少可以通过longjmp()把返回值
    * 记在jmp_buf中，然后setjmp()要返回时去读jmp_buf中对应的位置。
    * 
    * 这里要调setjmp是libpng官方的要求，如果libpng的函数出现问题，会调longjmp()以期望返回上层函数，实现异常处理，
    * "When libpng encounters an error, it expects to longjmp back to your routine. Therefore, you will need to call setjmp and pass your png_jmpbuf(png_ptr)"
    * 
    * 从这里开始，到下一次重新setjmp()，如果调libpng的函数有异常，会返回到这里的setjmp()，并且这次setjmp()返回值会变
    */
    if (int pngret = setjmp(png_jmpbuf(png_ptr))) {
        fclose(fp);
        std::cout << "libpng longjmp() returned " << pngret << std::endl;
        return -6;
    }
    // 初始化
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    // 读取png信息
    png_read_info(png_ptr, info_ptr);
    png_uint_32 png_width = png_get_image_width(png_ptr, info_ptr); // 图片宽度
    png_uint_32 png_height = png_get_image_height(png_ptr, info_ptr); // 图片高度
    png_byte color_type = png_get_color_type(png_ptr, info_ptr); // 图片颜色类型，例如灰度图像，RGB彩色图像，带Alpha通道的RGB彩色图像

    // 在这里设置catch
    if (int pngret = setjmp(png_jmpbuf(png_ptr))) {
        fclose(fp);
        std::cout << "libpng longjmp() returned " << pngret << std::endl;
        return -7;
    }

    png_bytep* png_data; // 这里png_data是二级指针
    png_data = (png_bytep*)malloc(sizeof(png_bytep) * png_height); // 准备了height个png_bytep，png_data指向第一个png_bytep
    for (int i = 0; i < png_height; i++) {
        png_data[i] = (png_bytep)malloc(png_get_rowbytes(png_ptr, info_ptr));
    }
    png_read_image(png_ptr, png_data); // 这里将图片的像素读到了png_data中

    /* 把png图片处理成28 * 28的。[0, w] ×[0, h]变成[0, INPUT_WIDTH] ×[0, INPUT_HEIGHT]，
    变换后的点(u, v)的值取[u * w / INPUT_WIDTH, (u + 1) * w / INPUT_WIDTH) × [v * h / INPUT_HEIGHT, (v + 1) * h / INPUT_HEIGHT)内的平均值
    */
    // 注意u为行号，其范围为[0, INPUT_HEIGHT)而非INPUT_WIDTH
    for (int u = 0; u < INPUT_HEIGHT; u++) {
        for (int v = 0; v < INPUT_WIDTH; v++) {
            float value = 0;
            int count = 0;
            for (int i = u * png_width / INPUT_HEIGHT; i < (u + 1) * png_width / INPUT_HEIGHT; i++) {
                for (int j = v * png_height / INPUT_WIDTH; j < (v + 1) * png_height / INPUT_WIDTH; j++) {
                    value += get_float_color_in_png(i, j, png_width, png_height, color_type, png_data);
                    count++;
                }
            }
            input_image[u * INPUT_HEIGHT + v] = value / count;
        }
    }
    int digit = num_recognizer_recognize(recognizer, input_image.data(), result);
    for (int i = 0; i < png_height; i++) {
        free(png_data[i]);
    }
    free(png_data);
    return digit;
}

}