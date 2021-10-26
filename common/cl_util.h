#pragma once

#include <CL/cl.h>

typedef struct {

	cl_device_id device;
	cl_platform_id platform;
	
} scl_device_platform_t;


/**
 * Look after and return a device
 * with the most OpenCL compute units
 * to engage in.
 */
scl_device_platform_t scl_get_device();

cl_program scl_build_program_inline(
		cl_context context,
		cl_device_id device,
		const char *source,
		size_t source_len,
		cl_int *err
);
