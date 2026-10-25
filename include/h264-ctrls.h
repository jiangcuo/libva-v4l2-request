/* SPDX-License-Identifier: GPL-2.0 */
/* Use the stateless codec controls from the system kernel headers. */
#ifndef _H264_CTRLS_H_
#define _H264_CTRLS_H_

#include <linux/videodev2.h>

#ifndef V4L2_PIX_FMT_H264_SLICE
#define V4L2_PIX_FMT_H264_SLICE v4l2_fourcc('S', '2', '6', '4')
#endif
/* Name used by this driver for the same pixel format. */
#define V4L2_PIX_FMT_H264_SLICE_RAW V4L2_PIX_FMT_H264_SLICE

#endif
