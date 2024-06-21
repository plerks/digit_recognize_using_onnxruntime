#include <num_recognizer.h>
#include <stdio.h>

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        printf("wrong usage, run like `./build/Debug/recognize.exe models/mnist-8.onnx 2.png`\n");
        return -1;
    }
    int result = -1; // 识别结果
    Recognizer *recognizer;
    num_recognizer_init();
    num_recognizer_create(argv[1], &recognizer);
    int ret = num_recognizer_recognize_png(recognizer, argv[2], &result);
    if (ret != 0) {
        printf("Failed to recognize\n");
        num_recognizer_delete(recognizer);
        return ret;
    }
    printf("%d\n", result);
    fflush(stdout);
    num_recognizer_delete(recognizer);
    return ret;
}
