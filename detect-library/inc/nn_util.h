/****************************************************************************
 *
 *    Copyright (c) 2022 amlogic Corporation
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a
 *    copy of this software and associated documentation files (the "Software"),
 *    to deal in the Software without restriction, including without limitation
 *    the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *    and/or sell copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *    DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/

#ifndef _NN_UTIL_H
#define _NN_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_IDLE          0
#define POWER_ON            1
#define POWER_SUSPEND       2
#define POWER_OFF           3
#define POWER_RESET         4

typedef struct{
    float x, y;
} landmark;

typedef struct{
    float x, y, w, h, prob_obj;
} box;

typedef struct plate_box_{
    float x, y, w, h;
}plate_box;

typedef struct{
    int index;
    int classId;
    float **probs;
} sortable_bbox;

typedef struct{
    int index;
    int classId;
    float probs;  //**probs to probs
} sortable_bbox_plate;

//////////////////////nn sdk demo struct type//////////////////
typedef struct __box
{
    float x;
    float y;
    float w;
    float h;
    float score;
    float objectClass;
}detBox;

typedef struct __point
{
    float x;
    float y;
}point_t;

typedef struct __nn_image_out
{
    int height;
    int width;
    int channel;
    unsigned char *data;  //this buffer is returned by aml_module_output_get
}image_out_t;

///////////////////////////////////////some util api///////////////////////////////////////////////
int init_fb(void);
void *camera_thread_func(void *arg);
int sysfs_control_read(const char* name,char *out);
int sysfs_control_write(const char* pname,char *value);
int findtok(const char *buff,const char token,int length);
void activate_array(float *start, int num);
int entry_index(int lw, int lh, int lclasses, int loutputs, int batch, int location, int entry);
int mate(const char *src,const char *match, unsigned int size);


#ifdef __cplusplus
} //extern "C"
#endif
#endif