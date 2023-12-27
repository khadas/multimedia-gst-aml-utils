/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * aml_nnsdk
 */

#ifndef _POSTPROCESS_UTIL_H
#define _POSTPROCESS_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif


// post process int8 to float32
void pp_dtypeToF32(nn_output *out_data, nn_output *in_data);

void pp_nhwc_to_nchw(nn_output *output_data);

void pp_nchw_f32(nn_output *out_data, nn_output *in_data);
void pp_nchw_f32_onescale(nn_output *out_data, nn_output *in_data, int index);

void copy_buf(nn_output *out_data, nn_output *in_data);



#ifdef __cplusplus
} //extern "C"
#endif
#endif // _POSTPROCESS_UTIL_H
