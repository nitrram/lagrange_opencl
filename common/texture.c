#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*OpenCL*/

#pragma OPENCL CL_KHR_gl_sharing


#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <CL/cl_egl.h>

/*OpenGL*/
#include <GL/glew.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/freeglut.h>

/*EGL*/
#include <EGL/egl.h>

#include <GLFW/glfw3.h>

#include <time.h>

#include "cl_prog.h"

#define DATA_DENSITY 8


typedef struct {
  cl_device_id device;
  cl_platform_id platform;
} device_platform_t;

static device_platform_t create_device() {

  cl_int err;
  cl_uint num_platforms;
  err = clGetPlatformIDs(0, NULL, &num_platforms);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "Cannot get the number of OpenCL platforms available.\n");
    exit(EXIT_FAILURE);
  }

  printf("Num of platforms: %d\n", (int)num_platforms);
  cl_platform_id platforms[num_platforms];
  err = clGetPlatformIDs(num_platforms, platforms, NULL);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "Cannot get  platform ids.\n");
    exit(EXIT_FAILURE);
  }

  int i = 0;
  cl_platform_id platform;
  char *platform_name;
  size_t platform_name_size;
  cl_uint num_devices;
  cl_device_id device;
  while(i < num_platforms) {

    platform = platforms[i++];

    clGetPlatformInfo(platform, CL_PLATFORM_NAME, 0, NULL, &platform_name_size);
    clGetPlatformInfo(platform, CL_PLATFORM_NAME, platform_name_size, platform_name, NULL);
    printf("\033[0;32mPlatform name:\033[0;0m [%d]%s\n", platform_name_size, platform_name);

    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);
    if(err != CL_SUCCESS) {
      fprintf(stderr, "Cannot find any GPU device on the platform %s.\n", platform_name);
      continue;
    }
    printf("Num of devices: %d\n", num_devices);

    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if(err < 0) {
      fprintf(stderr, "\033[0;31mCould not find andy GPU specific device - falling back to CPU\033[0;0m");
      continue;
    } else {
      return (device_platform_t){device, platform};
    }
  }

  exit(EXIT_FAILURE);

  return (device_platform_t){NULL,NULL};
}

static cl_program build_program_inline(cl_context context, cl_device_id device, const char *source, size_t source_len) {

  char *program_log;
  size_t log_size;
  cl_int err;

  printf("program size: %d [bytes]\n", source_len);

  cl_program program;
  program = clCreateProgramWithSource(context, 1, (const char **)&source, &source_len, &err);
  if(err < 0) {
    fprintf(stderr, "error creating program from source: %d", err);
  }

  err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  if(err < 0) {
    /* Find size of log and print to std output */
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
        0, NULL, &log_size);
    program_log = (char*) malloc(log_size + 1);
    program_log[log_size] = '\0';
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
        log_size + 1, program_log, NULL);
    printf("%s\n", program_log);
    free(program_log);
  }

  return program;
}

static float *generate_net(size_t wdth){

  size_t net_len = 4 *DATA_DENSITY*DATA_DENSITY;

  float *output = (float *)malloc(sizeof(float) * net_len + 1);
  uint32_t x,y,n;
  int i;

  srand((unsigned)time(0));
  n = wdth / DATA_DENSITY;

  //printf("gen_net: %d [%d] ~ %d (%d)\n", n, i, x, sizeof(uint32_t) * 4 * DATA_DENSITY * DATA_DENSITY + 1);
  for(x = 0, y = 0, i = 0; i < net_len; x+=n, i+=4) {
    output[i] = (float)(x % wdth);       /* x */
    if(output[i] == 0) {
      if(i != 0) {
  printf("\n");
  y+=n;
      }
    }
    output[i+1] = (float)y;            /* y */
    output[i+2] = (float)((rand() % 200)); // << ((rand() % 2) ? 1 : 0));  /* z */

    printf("[%3d %3d %4.1f] ", x%wdth, y, output[i+2]);
  }
  printf("\n");

  return output;
}

static double now_ms() {
  struct timespec res;
  clock_gettime(CLOCK_REALTIME, &res);
  return 1000.0*res.tv_sec + (double)res.tv_nsec/1e6;
}

static cl_mem configure_shared_data(cl_context context, GLuint *pbo, size_t size) {

  int err;

  glGenBuffers(1, pbo);
  glBindBuffer(GL_ARRAY_BUFFER, *pbo);
  glBufferData(GL_ARRAY_BUFFER, size*sizeof(unsigned char), NULL, GL_DYNAMIC_DRAW);

  cl_mem result = clCreateFromGLBuffer(context, CL_MEM_WRITE_ONLY, *pbo, &err);
  if( err < 0 ){
    fprintf(stderr, "Could not create pixel buffer\n");
  }

  return result;
}


GLuint _pixel_gl;

static float *_net;
static float *_dim_x;

static const size_t _net_len = 4 * DATA_DENSITY * DATA_DENSITY;
static unsigned int _size_r;
static unsigned int _size;
static unsigned int _dens_arg;
static size_t _dim_x_siz;

static cl_kernel _dim_x_kernel;
static cl_kernel _dim_xy_kernel;

static cl_device_id _device;
static cl_context _context;
static cl_program _program;
static cl_command_queue _queue;

static cl_mem _net_buf;
static cl_mem _dim_x_buf;
static cl_mem _pixel_buf;

static size_t _wdth;
static size_t _hght;


void update_texture() {

  cl_int err;
  cl_event dim_xy_event, dim_x_event;

  size_t wrk_group_enh[] = { DATA_DENSITY, DATA_DENSITY };
  size_t wrk_units_enh[] = { _wdth*DATA_DENSITY, _hght*DATA_DENSITY };
  size_t wrk_units[] = { _wdth, _hght };
  size_t wrk_unit_wdth = _wdth;

  float *net_modified = (float *)clEnqueueMapBuffer(_queue, _net_buf, CL_TRUE, CL_MAP_WRITE, 0, _net_len*4, 0, NULL, NULL, &err);

  for(int i = 0; i < _net_len; i+=4) {
    net_modified[i+2] += (float)((rand() % 10) * ((rand() % 2) ? 1 : -1));
  }

  err = clEnqueueUnmapMemObject(_queue, _net_buf, net_modified, 0, NULL, NULL);
  if(err < 0) {
    fprintf(stderr, "Could not unmap buffer: %d\n", err);
  }

  clEnqueueBarrier(_queue);

  clSetKernelArg(_dim_x_kernel, 0, sizeof(cl_mem), &_net_buf);
  err = clEnqueueNDRangeKernel(_queue, _dim_x_kernel, 1, NULL, &wrk_unit_wdth, NULL, 0, NULL, &dim_x_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not enqueue kernel dim1: %d\n", err);
  }

  err = clWaitForEvents(1, &dim_x_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not wait for events\n");
  }


  glFinish();

  err = clEnqueueAcquireGLObjects(_queue, 1, &_pixel_buf, 0, NULL, NULL);
  if( err < 0 ) {
     fprintf(stderr, "Could not acquire GL objects\n");
  }


  //  printf("wrk_units: [%d %d]\n",wrk_units[0], wrk_units[1]);
  clSetKernelArg(_dim_xy_kernel, 0, sizeof(cl_mem), &_dim_x_buf);
  err = clEnqueueNDRangeKernel(_queue, _dim_xy_kernel, 2, NULL, wrk_units, NULL, 0, NULL, &dim_xy_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not enqueue kernel: %d\n", err);
  }

  err = clWaitForEvents(1, &dim_xy_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not wait for events\n");
  }

  // clEnqueueBarrier(_queue);
  // unsigned char *test = (unsigned char*)malloc(_size);
  // printf("testing reading buffer of size: %d\n", _size);
  // err =  clEnqueueReadBuffer(_queue, _pixel_buf, CL_TRUE, 0, _size, test, 0, NULL, NULL);
  // if(err < 0) {
  //   fprintf(stderr, "Could not enqueue reading inter: %d\n", err);
  // }
  // //  printf("carry %d\n", clEnqueueBarrier(_queue));
  // for(int i=10024; i<10024+48 ; i+=4) {
  //   printf("[%3d %3d %3d]\n", test[i], test[i+1], test[i+2]);
  // }
  // printf("\n");


  clEnqueueReleaseGLObjects(_queue, 1, &_pixel_buf, 0, NULL, NULL);

  clFinish(_queue);
  clReleaseEvent(dim_xy_event);
  clReleaseEvent(dim_x_event);


  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pixel_gl);

  // last arg set to 0
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _wdth, _hght, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
  glActiveTexture(GL_TEXTURE0);
}

void free_texture() {
  clReleaseMemObject(_dim_x_buf);
  clReleaseMemObject(_pixel_buf);
  clReleaseKernel(_dim_xy_kernel);
  clReleaseKernel(_dim_x_kernel);
  clReleaseCommandQueue(_queue);
  clReleaseProgram(_program);
  clReleaseContext(_context);
}


GLuint generate_texture(const uint32_t wdth, const uint32_t hght) {

  cl_int err;

  _wdth = wdth;
  _hght = hght;

  device_platform_t dev_plat = create_device();

  _device = dev_plat.device;


  if(!eglGetCurrentContext()) {
    fprintf(stderr, "\033[0;35mNo current EGL context available \033[0;0m\n");
  }

  if(!eglGetCurrentDisplay()) {
    fprintf(stderr, "\033[0;35mNo current EGL display available \033[0;0m\n");
  }

  if(!glXGetCurrentContext()) {
    fprintf(stderr, "\033[0;35mNo current GLX context available \033[0;0m\n");
  }

  if(!glXGetCurrentDisplay()) {
    fprintf(stderr, "\033[0;35mNo current GLX display available \033[0;0m\n");
  }


  if( glXIsDirect(glXGetCurrentDisplay(), glXGetCurrentContext()) ) {
    fprintf(stderr, "\033[0;31mGLX context is direct rendering \033[0;0m\n");
  }



  cl_context_properties context_properties[] = {
    CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
    CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
    CL_CONTEXT_PLATFORM, (cl_context_properties) dev_plat.platform,
    0};


  /*

  cl_context_properties context_properties[] = {
    CL_GL_CONTEXT_KHR, (cl_context_properties) eglGetCurrentContext(),
    CL_EGL_DISPLAY_KHR, (cl_context_properties) eglGetCurrentDisplay(),
    CL_CONTEXT_PLATFORM, (cl_context_properties) dev_plat.platform,
    0};

  */

  //  glXMakeCurrent(NULL, NULL, NULL);
  // -1000    CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR    clGetGLContextInfoKHR, clCreateContext    CL and GL not on the same device (only when using a GPU).
  _context = clCreateContext(context_properties, 1, &_device, NULL, NULL, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create context [%d]\n", err);
  }

  _net = generate_net(wdth);


  srand((unsigned)time(0));

  _program = build_program_inline(_context, _device, (const char *)opencl_program_source, opencl_program_source_len);

  _size_r = wdth*hght;
  _size = _size_r*4;

  _dens_arg = DATA_DENSITY;
  _dim_x_siz = sizeof(float) * 4 * DATA_DENSITY * wdth;
  _dim_x = (float*)malloc(_dim_x_siz);
  _dim_x_buf = clCreateBuffer(_context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, _dim_x_siz, _dim_x, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create buffer of the tmp_buf [%d]\n", err);
  }

  _dim_x_kernel = clCreateKernel(_program, "dim_x", &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create kernel dim1 [%d]\n", err);
  }

  _net_buf = clCreateBuffer(_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
          _net_len * sizeof(float), _net, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create buffer of the net [%d]\n", err);
  }
  clSetKernelArg(_dim_x_kernel, 0, sizeof(cl_mem), &_net_buf);
  clSetKernelArg(_dim_x_kernel, 1, sizeof(cl_mem), &_dim_x_buf);
  clSetKernelArg(_dim_x_kernel, 2, sizeof(unsigned int), &_dens_arg);

  _dim_xy_kernel = clCreateKernel(_program, "dim_xy", &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create kernel [%d]\n", err);
  }

  _pixel_buf = configure_shared_data(_context, &_pixel_gl, _size);
  clSetKernelArg(_dim_xy_kernel, 0, sizeof(cl_mem), &_dim_x_buf);
  clSetKernelArg(_dim_xy_kernel, 1, sizeof(cl_mem), &_pixel_buf);
  clSetKernelArg(_dim_xy_kernel, 2, sizeof(unsigned int), &_dens_arg);


  _queue = clCreateCommandQueue(_context, _device, 0, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create queue [%d]\n", err);
  }
  //  printf("tmp_x: %d\n", (int)_dim_x_siz);

  glFinish();

  err = clEnqueueAcquireGLObjects(_queue, 1, &_pixel_buf, 0, NULL, NULL);
  if( err < 0 ) {
    fprintf(stderr, "Could not acquire GL objects [%d]\n", err);
  }
  printf("[WxH]%dx%d\n", wdth, hght);

  cl_event dim_xy_event, dim_x_event;
  size_t wrk_units[] = { wdth, hght };
  size_t wrk_unit_wdth = (size_t)wdth;

  err = clEnqueueNDRangeKernel(_queue, _dim_x_kernel, 1, NULL, &wrk_unit_wdth, NULL, 0, NULL, &dim_x_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not enqueue kernel dim1: %d\n", err);
  }

  // float* test = (float*)malloc(tmp_siz);
  // printf("testing reading buffer of size: %d\n", tmp_siz);
  // err =  clEnqueueReadBuffer(queue, tmp_buf, CL_TRUE, 0, tmp_siz, test, 0, NULL, NULL);
  // if(err < 0) {
  //     fprintf(stderr, "Could not enqueue reading inter: %d\n", err);
  // }
  // printf("carry %d\n", clEnqueueBarrier(queue));
  // for(int i=0; i< 32; i+=4) {
  //   printf("%7.1f ", test[i+1]);
  // }
  // printf("\n");


  err = clEnqueueNDRangeKernel(_queue, _dim_xy_kernel, 2, NULL, wrk_units, NULL, 1, &dim_x_event, &dim_xy_event);
  if( err < 0 ) {
     fprintf(stderr, "Could not enqueue kernel: %d\n", err);
  }

  err = clWaitForEvents(1, &dim_xy_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not wait for events\n");
  }

  clEnqueueReleaseGLObjects(_queue, 1, &_pixel_buf, 0, NULL, NULL);
  clFinish(_queue);
  clReleaseEvent(dim_xy_event);
  clReleaseEvent(dim_x_event);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pixel_gl);

  // last arg set to 0
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wdth, hght, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
  glActiveTexture(GL_TEXTURE0);

  GLuint textureID;
  glGenTextures(1, &textureID);

  // ... nice trilinear filtering.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glGenerateMipmap(GL_TEXTURE_2D);


  // Return the ID of the texture we just created
  return textureID;
}
