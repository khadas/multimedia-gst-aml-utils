/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>
#include "jpeg.h"

struct aml_jpeg_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct aml_jpeg_error_mgr * error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a aml_jpeg_error_mgr struct, so coerce pointer */
  error_ptr err = (error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(err->setjmp_buffer, 1);
}

bool create_jpeg (unsigned char *image_buffer,
    int image_width, int image_height, int quality,
    bool dest_to_mem, unsigned char **outbuf, unsigned long *out_size,
    char *filename)
{
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  struct jpeg_compress_struct cinfo;
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct aml_jpeg_error_mgr jerr;
  /* More stuff */
  FILE * outfile = NULL;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */
  unsigned char *mem = NULL;
  unsigned long mem_size = 0;

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = error_exit;
  /* Establish the setjmp return context for error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_compress(&cinfo);
    if (outfile) {
      fclose(outfile);
    }
    return false;
  }
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  if (dest_to_mem) {
    jpeg_mem_dest(&cinfo, &mem, &mem_size);
  } else {
    /* Here we use the library-supplied code to send compressed data to a
     * stdio stream.  You can also write your own code to do something else.
     * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
     * requires it in order to write binary files.
     */
    if ((outfile = fopen(filename, "wb")) == NULL) {
      fprintf(stderr, "can't open %s\n", filename);
      return false;
    }
    jpeg_stdio_dest(&cinfo, outfile);
  }

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = image_width; 	/* image width and height, in pixels */
  cinfo.image_height = image_height;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = image_width * 3;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  if (dest_to_mem) {
    if (outbuf) *outbuf = mem;
    if (out_size) *out_size = mem_size;
  } else {
    /* After finish_compress, we can close the output file. */
    fclose(outfile);
  }

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

  /* And we're done! */
  return true;
}

unsigned long generate_jpeg_to_mem (unsigned char *imgbuf, int w, int h, int quality, unsigned char **outbuf) {
  unsigned long out_size = 0;
  if (create_jpeg(imgbuf, w, h, quality, true, outbuf, &out_size, NULL)) {
    return out_size;
  }
  return out_size;
}

bool generate_jpeg_to_file (unsigned char *imgbuf, int w, int h, int quality, char *filename) {
  return create_jpeg(imgbuf, w, h, quality, false, NULL, NULL, filename);
}

bool jpeg_to_rgb888(char *filename, int *pwidth, int *pheight, int *pstride,
                    unsigned char *outbuf) {
  bool ret = false;
  if (NULL == filename)
    return ret;

  struct jpeg_decompress_struct cinfo;
  struct aml_jpeg_error_mgr jerr;
  FILE *fp;

  if ((fp = fopen (filename, "rb")) == NULL) {
    return ret;
  }

  cinfo.err = jpeg_std_error (&jerr.pub);
  jerr.pub.error_exit = error_exit;
  /* Establish the setjmp return context for error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(&cinfo);
    if (fp) {
      fclose(fp);
    }
    return NULL;
  }
  jpeg_create_decompress(&cinfo);

  jpeg_stdio_src (&cinfo, fp);

  if (JPEG_HEADER_OK != jpeg_read_header (&cinfo, true)) {
    goto bail;
  }

  int stride = ((cinfo.image_width * cinfo.num_components + 31) >> 5) << 5;
  int align_width = stride / cinfo.num_components;

  if (outbuf == NULL) {
    ret = true;
    goto skip_decompress_exit;
  }

  unsigned char *rdata = outbuf;

  if (!jpeg_start_decompress (&cinfo)) {
    goto bail;
  }

  JSAMPROW row_pointer[1];

  while (cinfo.output_scanline < cinfo.output_height)
  {
    row_pointer[0] =
      &rdata[cinfo.output_scanline * stride];
    jpeg_read_scanlines (&cinfo, row_pointer, 1);
  }

  jpeg_finish_decompress(&cinfo);

  ret = true;

skip_decompress_exit:

  if (pwidth) *pwidth = align_width;
  if (pheight) *pheight = cinfo.image_height;
  if (pstride) *pstride = stride;

bail:
  fclose (fp);
  jpeg_destroy_decompress(&cinfo);

  return ret;
}
