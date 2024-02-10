#include "cl_util.h"

#include <stdio.h>

#define E_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define E_WARN(...) E_ERROR(__VA_ARGS__)

scl_device_platform_t scl_get_device() {

  cl_int err;
  cl_uint nplat;

  scl_device_platform_t dev_plat = {0, 0};

  err = clGetPlatformIDs(0, NULL, &nplat);
  if(err != CL_SUCCESS) {
    E_ERROR("Cannot obtain the count of OpenCL platforms (%d)\n", (int)err);
    return dev_plat;
  }

  cl_platform_id platforms[nplat];
  err = clGetPlatformIDs(nplat, platforms, NULL);
  if(err != CL_SUCCESS) {
    E_ERROR("Cannot obtain platform ids (%d)\n", err);
    return dev_plat;
  }

  /*      CL_DEVICE_MAX_COMPUTE_UNITS */
  cl_uint max_compute_units = 0;
  for(int i=0; i < nplat; ++i) {

    cl_platform_id curplat = platforms[i];
    cl_uint ndev;
    err = clGetDeviceIDs(curplat, CL_DEVICE_TYPE_ALL, 0, NULL, &ndev);
    if(err != CL_SUCCESS) {
      E_WARN("Cannot obtain platform[%d]'s count of devices.\n", i);
      continue;
    }

    cl_device_id curdevs[ndev];
    err = clGetDeviceIDs(curplat, CL_DEVICE_TYPE_ALL, ndev, curdevs, NULL);
    if(err != CL_SUCCESS) {
      E_WARN("Cannot obtain platform[%d]'s %d devices.\n", i, ndev);
      continue;
    }

    for(int j=0; j < ndev; ++j) {
      cl_device_id curdev = curdevs[j];
      cl_uint cur_max_compute_units;
      err = clGetDeviceInfo(curdev, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &cur_max_compute_units, NULL);
      if(err != CL_SUCCESS) {
        E_WARN("Cannot get platform[%d] device[%d] info.\n", i, j);
        continue;
      }

      if(cur_max_compute_units > max_compute_units) {
        max_compute_units = cur_max_compute_units;
        dev_plat = (scl_device_platform_t){ .device = curdev, .platform = curplat };
      }

    }
  }

  if(max_compute_units <= 0 && err != CL_SUCCESS) {
    E_ERROR("Could not find any convenient platform having a suitable OpenCL device\n");
  }

  return dev_plat;
}


cl_program scl_build_program_inline(
                                    cl_context context,
                                    cl_device_id device,
                                    const char *source,
                                    size_t source_len,
                                    cl_int *err
                                    ) {

  char *program_log;
  size_t log_size;

  cl_program program;
  program = clCreateProgramWithSource(context, 1, (const char **)&source, &source_len, err);

  if(*err < 0) {
    return program;
  }

  *err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  if(*err < 0) {
    /* Find size of log and print to std output */
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                          0, NULL, &log_size);
    program_log = (char*) malloc(log_size + 1);
    program_log[log_size] = '\0';
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                          log_size + 1, program_log, NULL);
    fprintf(stderr, "%s\n", program_log);
    free(program_log);
  }

  return program;
}
