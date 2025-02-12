#ifndef _DETECT_COMMON_HH_
#define _DETECT_COMMON_HH_
#include "nn_detect_utils.h"
typedef unsigned char uint8_t;

typedef int det_status_t;

/// pixel format definition
typedef enum {
  ///< Y 1 8bpp(Single channel 8bit gray pixels )
  PIX_FMT_GRAY8,
  ///< YUV  4:2:0 12bpp ( 3 channels, one brightness channel, the othe
  /// two for the U component and V component channel, all channels are continuous)
  PIX_FMT_YUV420P,
  ///< YUV  4:2:0 12bpp ( 2 channels, one channel is a continuous
  /// luminance
  /// channel, and the other channel is interleaved as a UV component )
  PIX_FMT_NV12,
  ///< YUV  4:2:0   	12bpp ( 2 channels, one channel is a continuous
  /// luminance
  /// channel, and the other channel is interleaved as a UV component )
  PIX_FMT_NV21,
  ///< BGRA 8:8:8:8 	32bpp ( 4-channel 32bit BGRA pixels )
  PIX_FMT_BGRA8888,
  ///< BGR  8:8:8   	24bpp ( 3-channel 24bit BGR pixels )
  PIX_FMT_BGR888,
  ///< RGBA 8:8:8：8	32bpp ( 4-channel 32bit RGBA pixels )
  PIX_FMT_RGBA8888,
  ///< RGB  8:8:8		24bpp ( 3-channel 24bit RGB pixels )
  PIX_FMT_RGB888
} det_pixel_format;

typedef struct rect_point_t{
	float left;   ///< The value of left direction of the rectangle
	float top;    ///< The value of top direction of the rectangle
	float right;  ///< The value of right direction of the rectangle
	float bottom; ///< The value of bottom direction of the rectangle
	float score;  ///< The value of score of the rectangle
} det_rect_point_t;

typedef struct circle_point_t{
	float center;   ///<circle ceter point
	float radius;   ///<radius
    float score;    ///<The value of score of the score
} det_circle_point_t;

/// float type point definition
typedef struct pointf_t {
  float x;                  ///< The float value of the horizontal direction of the point
  float y;                  ///< The float value of the vertical direction of the point
} det_pointf_t;

typedef struct single_point_t{
	float x;        ///<point x value
	float y;        ///<point y value
	float param;    ///<point param value if need
} det_single_point_t;

typedef struct input_image {
  unsigned char *data;            ///<picture data ptr value
  det_pixel_format pixel_format;  ///< color format
  int width;                      ///< width value of pixel
  int height;                     ///< height value of pixel
  int channel;                    ///< stride or channel for picture
  det_pointf_t  inPoint[5];
} input_image_t;

typedef enum {
	DET_SINGLEPOINT_TYPE = 1,
	DET_RECTANGLE_TYPE = 2,
	DET_CIRCLE_TYPE = 3,
	DET_IMAGE_TYPE = 4,
} det_position_type;

typedef enum {
	DET_YOLOFACE_V2 = 0,
	DET_YOLO_V2,
	DET_YOLO_V3,
	DET_YOLO_TINY,
	DET_SSD,
	DET_MTCNN_V1,
	DET_MTCNN_V2,
	DET_FASTER_RCNN,
	DET_DEEPLAB_V1,
	DET_DEEPLAB_V2,
	DET_DEEPLAB_V3,
	DET_FACENET,
	DET_AML_FACE_RECOGNITION,
	DET_AML_FACE_DETECTION,
	DET_AML_PERSON_DETECTION,
  DET_AML_FACE_DETECT_RFB,
	DET_BUTT,
} det_model_type;

typedef struct
{
    float floatX[5];
    float floatY[5];
}face_pts;

/// point definition
typedef struct det_position_t {
  det_position_type type;
	union {
		det_rect_point_t rectPoint;
		det_circle_point_t circlePoint;
		det_single_point_t singlePoint;
		input_image_t imageData;
	}point;
  face_pts tpts;
  char reserved[4];
} det_position_float_t;

///integer type point definition
typedef struct pointi_t {
  int x;                   ///< The integer value of the horizontal direction of the point
  int y;                   ///< The integer value of the vertical direction of the point
} det_pointi_t;

///Classification of label results
typedef struct classify_result_t {
  int label_id;                        ///> Classification of label ID
  char label_name[MAX_LABEL_LENGTH];   ///> Label name
} det_classify_result_t;


///faec orientation
typedef enum {
  DET_FACE_UP = 1,              ///< Face up
  DET_FACE_LEFT = 2,            ///< Face left, the face was rotated counterclockwise by 90 degrees
  DET_FACE_DOWN = 4,            ///< Face down, the face was rotated counterclockwise by 180 degrees
  DET_FACE_RIGHT = 8,           ///< Face right, the face was rotated counterclockwise by 270 degrees
  DET_FACE_UNKNOWN = 0xf        ///< Face detection unknown, API automatically determines the face
                                /// orientation, but use more time, face_track do not support this parameter
} det_face_orientation;

typedef enum {
  DEV_REVA = 0,
  DEV_REVB,
  DEV_SM1,
  DEV_A1,
  DEV_TM2,
  DEV_C2,
  DEV_BUTT,
  DEV_C308,
  DEV_AX201,
  DEV_A311D2,
  DEV_G12B,
} dev_type;

///@brief log output format
typedef enum{
  DET_LOG_NULL = -1,	        ///< close all log output
  DET_LOG_TERMINAL,	            ///< set log print to terminal
  DET_LOG_FILE,		            ///< set log print to file
  DET_LOG_SYSTEM                ///< set log print to system
}det_log_format_t;

///@brief debug level
typedef enum{
	DET_DEBUG_LEVEL_RELEASE = -1,	///< close debug
	DET_DEBUG_LEVEL_ERROR,		    ///< error level,hightest level system will exit and crash
	DET_DEBUG_LEVEL_WARN,		    ///< warning, system continue working,but something maybe wrong
	DET_DEBUG_LEVEL_INFO,		    ///< info some value if needed
	DET_DEBUG_LEVEL_PROCESS,	    ///< default,some process print
	DET_DEBUG_LEVEL_DEBUG		    ///< debug level,just for debug
}det_debug_level_t;

typedef struct {
   int  detect_num;
   det_classify_result_t *result_name;
   det_position_float_t *point;
}det_detection_result_t;

typedef struct {
  uint8_t uint8[512];
}det_recognition_result_t;

typedef struct {
  int detect_num;
  char reserved[4];
} det_detection_param_t;

typedef struct {
  int length;
  float scale;
} det_recognition_param_t;

typedef struct {
  union{
    det_recognition_param_t reg_param;
    det_detection_param_t  det_param;
  } param;
}det_param_t;

typedef struct _DetectResult {
  union {
    det_recognition_result_t reg_result;
    det_detection_result_t  det_result;
  } result;
}DetectResult,*pDetResult;

#endif
