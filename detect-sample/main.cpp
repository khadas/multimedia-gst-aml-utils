#include <iostream>
#include <fstream>

#include "nn_detect.h"
#include "nn_detect_utils.h"

using namespace std;

#define _CHECK_STATUS_(status, stat, lbl) do {\
    if (status != stat) \
    { \
        cout << "_CHECK_STATUS_ File" << __FUNCTION__ << __LINE__ <<endl; \
    }\
    goto lbl; \
}while(0)

static void *read_file(const char *file_path, int *file_size)
{
    FILE *fp = NULL;
    int size = 0;
    void *buf = NULL;
    fp = fopen(file_path, "rb");
    if (NULL == fp)
    {
        printf("open file fail!\n");
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    buf = malloc(sizeof(unsigned char) * size);

    fread(buf, 1, size, fp);

    fclose(fp);

    *file_size = size;
    return buf;
}

int run_detect_model(int argc, char** argv)
{
    int ret = 0;
    int nn_height, nn_width, nn_channel, img_width, img_height;
    det_model_type type = DET_YOLOFACE_V2;
    pDetResult resultData;

    if (argc !=3) {
        cout << "input param error" <<endl;
        cout << "Usage: " << argv[0] << " type  picture_path"<<endl;
        return -1;
    }
    type = (det_model_type)atoi(argv[1]);

    char* picture_path = argv[2];
    det_set_log_config(DET_DEBUG_LEVEL_WARN,DET_LOG_TERMINAL);
    cout << "det_set_log_config Debug" <<endl;

    //prepare model
    ret = det_set_model(type);
    if (ret) {
        cout << "det_set_model fail. ret=" << ret <<endl;
        return ret;
    }
    cout << "det_set_model success!!" << endl;

    ret = det_get_model_size(type, &nn_width, &nn_height, &nn_channel);
    if (ret) {
        cout << "det_get_model_size fail" <<endl;
        return ret;
    }

    cout << "\nmodel.width:" << nn_width <<endl;
    cout << "model.height:" << nn_height <<endl;
    cout << "model.channel:" << nn_channel << "\n" <<endl;

    int input_size;
    input_image_t image;
    image.data      = (unsigned char *)read_file(picture_path, &input_size);
    image.width     = nn_width;
    image.height    = nn_height;
    image.channel   = nn_channel;
    image.pixel_format = PIX_FMT_RGB888;

    cout << "Det_set_input START" << endl;
    ret = det_set_input(image, type);
    if (ret) {
        cout << "det_set_input fail. ret=" << ret << endl;
        det_release_model(type);
        return ret;
    }
    cout << "Det_set_input END" << endl;

    cout << "Det_get_result START" << endl;
    resultData = (pDetResult)malloc(sizeof(*resultData));
    ret = det_get_result(resultData, type);
    if (ret) {
        cout << "det_get_result fail. ret=" << ret << endl;
        det_release_model(type);
        return ret;
    }
    cout << "Det_get_result END" << endl;

    det_release_model(type);
    return ret;
}

int main(int argc, char** argv)
{
    det_model_type type;
    if (argc < 3) {
        cout << "input param error" <<endl;
        cout << "Usage: " << argv[0] << " type  picture_path"<<endl;
        return -1;
    }

    type = (det_model_type)atoi(argv[1]);
    switch (type) {
        case DET_YOLO_V3:
            run_detect_model(argc, argv);
            break;
        case DET_AML_FACE_DETECTION:
            run_detect_model(argc, argv);
            break;
        default:
            cerr << "not support type=" << type <<endl;
            break;
    }

    return 0;
}
