/*
 * MPEG-2 and HEVC stateless decode are disabled: their kernel uAPI structs
 * changed after this driver was written and no longer match mainline
 * linux/v4l2-controls.h. Only the stable H.264 path is built.
 */
#include "mpeg2.h"
#include "h265.h"

int mpeg2_set_controls(struct request_data *driver_data,
		       struct object_context *context,
		       struct object_surface *surface_object)
{
	(void)driver_data;
	(void)context;
	(void)surface_object;
	return -1;
}

int h265_set_controls(struct request_data *driver_data,
		      struct object_context *context_object,
		      struct object_surface *surface_object)
{
	(void)driver_data;
	(void)context_object;
	(void)surface_object;
	return -1;
}
