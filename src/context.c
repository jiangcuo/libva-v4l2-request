/*
 * Copyright (C) 2007 Intel Corporation
 * Copyright (C) 2016 Florent Revest <florent.revest@free-electrons.com>
 * Copyright (C) 2018 Paul Kocialkowski <paul.kocialkowski@bootlin.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "context.h"
#include "config.h"
#include "request.h"
#include "surface.h"

#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include <mpeg2-ctrls.h>
#include <h264-ctrls.h>
#include <hevc-ctrls.h>

#include "utils.h"
#include "v4l2.h"

#include "autoconfig.h"

VAStatus RequestCreateContext(VADriverContextP context, VAConfigID config_id,
			      int picture_width, int picture_height, int flags,
			      VASurfaceID *surfaces_ids, int surfaces_count,
			      VAContextID *context_id)
{
	struct request_data *driver_data = context->pDriverData;
	struct object_config *config_object;
	struct object_surface *surface_object;
	struct object_context *context_object = NULL;
	struct video_format *video_format;
	VASurfaceID *ids = NULL;
	VAContextID id;
	VAStatus status;
	unsigned int output_type, capture_type;
	unsigned int i;
	int rc;

	video_format = driver_data->video_format;
	if (video_format == NULL)
		return VA_STATUS_ERROR_OPERATION_FAILED;

	output_type = v4l2_type_video_output(video_format->v4l2_mplane);
	capture_type = v4l2_type_video_capture(video_format->v4l2_mplane);

	config_object = CONFIG(driver_data, config_id);
	if (config_object == NULL) {
		status = VA_STATUS_ERROR_INVALID_CONFIG;
		goto error;
	}

	id = object_heap_allocate(&driver_data->context_heap);
	context_object = CONTEXT(driver_data, id);
	if (context_object == NULL) {
		status = VA_STATUS_ERROR_ALLOCATION_FAILED;
		goto error;
	}
	memset(&context_object->dpb, 0, sizeof(context_object->dpb));

	switch (config_object->profile) {

	case VAProfileH264Main:
	case VAProfileH264High:
	case VAProfileH264ConstrainedBaseline:
	case VAProfileH264MultiviewHigh:
	case VAProfileH264StereoHigh:
		break;

	default:
		status = VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
		goto error;
	}

	/*
	 * The OUTPUT format is set in RequestCreateConfig, before VAAPI
	 * allocates the surfaces that occupy the CAPTURE queue. Trying to
	 * change it here would return EBUSY on cedrus.
	 */

	/*
	 * Modern VAAPI applications pass no surface list to vaCreateContext:
	 * they allocate surfaces separately and hand the current one per
	 * picture. Adopt every existing surface.
	 */
	if (surfaces_count == 0) {
		struct object_surface *object;
		int iterator;

		object = (struct object_surface *)
			object_heap_first(&driver_data->surface_heap, &iterator);
		while (object != NULL) {
			surfaces_count++;
			object = (struct object_surface *)
				object_heap_next(&driver_data->surface_heap,
						 &iterator);
		}

		if (surfaces_count == 0) {
			status = VA_STATUS_ERROR_INVALID_SURFACE;
			goto error;
		}

		ids = malloc(surfaces_count * sizeof(VASurfaceID));
		if (ids == NULL) {
			status = VA_STATUS_ERROR_ALLOCATION_FAILED;
			goto error;
		}

		object = (struct object_surface *)
			object_heap_first(&driver_data->surface_heap, &iterator);
		for (i = 0; object != NULL; i++) {
			ids[i] = object->base.id;
			object = (struct object_surface *)
				object_heap_next(&driver_data->surface_heap,
						 &iterator);
		}
		surfaces_ids = ids;
	}

	/*
	 * The surface_ids array has been allocated by the caller and
	 * we don't have any indication wrt its life time. Let's make sure
	 * its life span is under our control.
	 */
	if (ids == NULL) {
		ids = malloc(surfaces_count * sizeof(VASurfaceID));
		if (ids == NULL) {
			status = VA_STATUS_ERROR_ALLOCATION_FAILED;
			goto error;
		}

		memcpy(ids, surfaces_ids, surfaces_count * sizeof(VASurfaceID));
	}

	for (i = 0; i < surfaces_count; i++) {
		surface_object = SURFACE(driver_data, surfaces_ids[i]);
		if (surface_object == NULL) {
			status = VA_STATUS_ERROR_INVALID_SURFACE;
			goto error;
		}
	}

	rc = v4l2_set_stream(driver_data->video_fd, output_type, true);
	if (rc < 0) {
		status = VA_STATUS_ERROR_OPERATION_FAILED;
		goto error;
	}

	rc = v4l2_set_stream(driver_data->video_fd, capture_type, true);
	if (rc < 0) {
		status = VA_STATUS_ERROR_OPERATION_FAILED;
		goto error;
	}

	context_object->config_id = config_id;
	context_object->render_surface_id = VA_INVALID_ID;
	context_object->surfaces_ids = ids;
	context_object->surfaces_count = surfaces_count;
	context_object->picture_width = picture_width;
	context_object->picture_height = picture_height;
	context_object->flags = flags;

	*context_id = id;

	status = VA_STATUS_SUCCESS;
	goto complete;

error:
	if (ids != NULL)
		free(ids);

	if (context_object != NULL)
		object_heap_free(&driver_data->context_heap,
				 (struct object_base *)context_object);

complete:
	return status;
}

VAStatus RequestDestroyContext(VADriverContextP context, VAContextID context_id)
{
	struct request_data *driver_data = context->pDriverData;
	struct object_context *context_object;
	struct video_format *video_format;
	unsigned int output_type, capture_type;
	int rc;

	video_format = driver_data->video_format;
	if (video_format == NULL)
		return VA_STATUS_ERROR_OPERATION_FAILED;

	output_type = v4l2_type_video_output(video_format->v4l2_mplane);
	capture_type = v4l2_type_video_capture(video_format->v4l2_mplane);

	context_object = CONTEXT(driver_data, context_id);
	if (context_object == NULL)
		return VA_STATUS_ERROR_INVALID_CONTEXT;

	rc = v4l2_set_stream(driver_data->video_fd, output_type, false);
	if (rc < 0)
		return VA_STATUS_ERROR_OPERATION_FAILED;

	rc = v4l2_set_stream(driver_data->video_fd, capture_type, false);
	if (rc < 0)
		return VA_STATUS_ERROR_OPERATION_FAILED;

	/*
	 * Surfaces are owned by the application (the frames context in
	 * modern VAAPI usage): it destroys them with vaDestroySurfaces.
	 */

	free(context_object->surfaces_ids);

	object_heap_free(&driver_data->context_heap,
			 (struct object_base *)context_object);

	rc = v4l2_request_buffers(driver_data->video_fd, output_type, 0);
	if (rc < 0)
		return VA_STATUS_ERROR_OPERATION_FAILED;

	rc = v4l2_request_buffers(driver_data->video_fd, capture_type, 0);
	if (rc < 0)
		return VA_STATUS_ERROR_OPERATION_FAILED;

	driver_data->capture_pool_size = 0;
	driver_data->capture_next = 0;
	driver_data->output_pool_size = 0;
	driver_data->output_next = 0;

	return VA_STATUS_SUCCESS;
}
