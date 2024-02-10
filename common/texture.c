#include <stdio.h>

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

#include "texture.h"
#include "cl_util.h"

/*Generated from Makefile*/
#include "cl_prog.h"

#define DATA_DENSITY 8

static const size_t _net_len = 4 * DATA_DENSITY * DATA_DENSITY;


static float *generate_net(size_t wdth){

  float *output = (float *)malloc(sizeof(float) * _net_len + 1);
  uint32_t x,y,n;
  int i;

  srand((unsigned)time(0));
  n = wdth / DATA_DENSITY;

  //printf("gen_net: %d [%d] ~ %d (%d)\n", n, i, x, sizeof(uint32_t) * 4 * DATA_DENSITY * DATA_DENSITY + 1);
  for(x = 0, y = 0, i = 0; i < _net_len; x+=n, i+=4) {
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
  cl_event dim_xy_event = 0, dim_x_event = 0, mapping_event = 0;

  size_t wrk_units[] = { _wdth, _hght };
  size_t wrk_unit_wdth = _wdth * DATA_DENSITY;
  size_t wrk_group_wdth = DATA_DENSITY;

  float *net_modified = (float *)clEnqueueMapBuffer(_queue, _net_buf, CL_TRUE, CL_MAP_WRITE, 0, _net_len*sizeof(float), 0, NULL, NULL, &err);

  for(int i = 0; i < _net_len; i+=4) {
    net_modified[i+2] += (float)((rand() % 10) * ((rand() % 2) ? 1 : -1));
  }

  err = clEnqueueUnmapMemObject(_queue, _net_buf, net_modified, 0, NULL, &mapping_event);
  if(err < 0) {
    fprintf(stderr, "Could not unmap buffer: %d\n", err);
    free_texture(all);
    return;
  }

  clEnqueueBarrierWithWaitList(_queue, 1 , &mapping_event, NULL);

  clSetKernelArg(_dim_x_kernel, 0, sizeof(cl_mem), &_net_buf);
  err = clEnqueueNDRangeKernel(_queue, _dim_x_kernel, 1, NULL, &wrk_unit_wdth, &wrk_group_wdth, 0, NULL, &dim_x_event);
  if( err < 0 ) {
    fprintf(stderr, "\nCould not enqueue kernel dim1: %d (wrkUnitWdth %d)\n", err, wrk_unit_wdth);
    if(dim_x_event) clReleaseEvent(dim_x_event);
    free_texture(all);
    return;
  }

  err = clWaitForEvents(1, &dim_x_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not wait for events\n");
    if(dim_x_event) clReleaseEvent(dim_x_event);
    free_texture(all);
    return;
  }

  glFinish();

  err = clEnqueueAcquireGLObjects(_queue, 1, &_pixel_buf, 0, NULL, NULL);
  if( err < 0 ) {
    fprintf(stderr, "Could not acquire GL objects\n");
    free_texture(all);
    return;
  }


  //  printf("wrk_units: [%d %d]\n",wrk_units[0], wrk_units[1]);
  clSetKernelArg(_dim_xy_kernel, 0, sizeof(cl_mem), &_dim_x_buf);
  err = clEnqueueNDRangeKernel(_queue, _dim_xy_kernel, 2, NULL, wrk_units, NULL, 0, NULL, &dim_xy_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not enqueue kernel: %d\n", err);
    if(dim_xy_event) clReleaseEvent(dim_xy_event);
    free_texture(all);
    return;
  }

  err = clWaitForEvents(1, &dim_xy_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not wait for events\n");
    if(dim_xy_event) clReleaseEvent(dim_xy_event);
    free_texture(all);
    return;
  }

  clEnqueueReleaseGLObjects(_queue, 1, &_pixel_buf, 0, NULL, NULL);

  clFinish(_queue);
  clReleaseEvent(mapping_event);
  clReleaseEvent(dim_xy_event);
  clReleaseEvent(dim_x_event);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pixel_gl);

  // last arg set to 0
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _wdth, _hght, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
  glActiveTexture(GL_TEXTURE0);
}

void free_texture(enum e_release type) {

  switch(type) {
  case all:
    clReleaseMemObject(_pixel_buf);
  case pixel:
    clReleaseCommandQueue(_queue);
  case queue:
    clReleaseKernel(_dim_xy_kernel);
  case dim_xy_kernel:
    clReleaseMemObject(_net_buf);
  case net:
    clReleaseKernel(_dim_x_kernel);
  case dim_x_kernel:
    clReleaseMemObject(_dim_x_buf);
  case dim_x:
    free(_dim_x);
  case program:
    if(_program) clReleaseProgram(_program);
    clReleaseContext(_context);
  case context:
    (void)(0);
  }
  free(_net);
}


GLuint generate_texture(const uint32_t wdth, const uint32_t hght) {

  cl_int err;

  _wdth = wdth;
  _hght = hght;

  scl_device_platform_t dev_plat = scl_get_device();

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

  _context = clCreateContext(context_properties, 1, &_device, NULL, NULL, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create context [%d]\n", err);
    free_texture(context);
    return 0;
  }

  _net = generate_net(wdth);

  srand((unsigned)time(0));

  _program = scl_build_program_inline(_context, _device, (const char *)opencl_program_source, opencl_program_source_len, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create OpenCL program [%d]\n", err);
    free_texture(program);
    return GL_INVALID_VALUE;
  }

  _size_r = wdth*hght;
  _size = _size_r*4;

  _dens_arg = DATA_DENSITY;
  _dim_x_siz = sizeof(float) * 4 * DATA_DENSITY * wdth;
  _dim_x = (float*)malloc(_dim_x_siz);
  _dim_x_buf = clCreateBuffer(_context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, _dim_x_siz, _dim_x, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create buffer of the tmp_buf [%d]\n", err);
    free_texture(dim_x);
    return GL_INVALID_VALUE;
  }

  _dim_x_kernel = clCreateKernel(_program, "dim_x", &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create kernel dim1 [%d]\n", err);
    free_texture(dim_x_kernel);
    return GL_INVALID_VALUE;
  }

  _net_buf = clCreateBuffer(_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                            _net_len * sizeof(float), _net, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create buffer of the net [%d]\n", err);
    free_texture(net);
    return GL_INVALID_VALUE;
  }

  clSetKernelArg(_dim_x_kernel, 0, sizeof(cl_mem), &_net_buf);
  clSetKernelArg(_dim_x_kernel, 1, sizeof(cl_mem), &_dim_x_buf);

  _dim_xy_kernel = clCreateKernel(_program, "dim_xy", &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create kernel [%d]\n", err);
    free_texture(dim_xy_kernel);
    return GL_INVALID_VALUE;
  }

  _pixel_buf = configure_shared_data(_context, &_pixel_gl, _size);
  clSetKernelArg(_dim_xy_kernel, 0, sizeof(cl_mem), &_dim_x_buf);
  clSetKernelArg(_dim_xy_kernel, 1, sizeof(cl_mem), &_pixel_buf);
  clSetKernelArg(_dim_xy_kernel, 2, sizeof(unsigned int), &_dens_arg);

  cl_queue_properties queue_properties[] = { CL_QUEUE_PROPERTIES, (const cl_queue_properties) (CL_QUEUE_ON_DEVICE && CL_QUEUE_PROFILING_ENABLE), 0 };
  _queue = clCreateCommandQueueWithProperties(_context, _device, queue_properties, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create queue [%d]\n", err);
    free_texture(queue);
    return GL_INVALID_VALUE;
  }
  //  printf("tmp_x: %d\n", (int)_dim_x_siz);

  glFinish();

  err = clEnqueueAcquireGLObjects(_queue, 1, &_pixel_buf, 0, NULL, NULL);
  if( err < 0 ) {
    fprintf(stderr, "Could not acquire GL objects [%d]\n", err);
    free_texture(queue);
    return GL_INVALID_VALUE;
  }
  printf("[WxH]%dx%d\n", _wdth, _hght);

  cl_event dim_xy_event = 0, dim_x_event = 0;
  size_t wrk_units[] = { _wdth, _hght };
  size_t wrk_unit_wdth = _wdth * DATA_DENSITY;
  size_t wrk_group_wdth = DATA_DENSITY;


  printf("[WuxHu]%dx%d\n", wrk_units[0], wrk_units[1]);

  err = clEnqueueNDRangeKernel(_queue, _dim_x_kernel, 1, NULL, &wrk_unit_wdth, NULL, 0, NULL, &dim_x_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not enqueue kernel dim1: %d\n", err);
    if(dim_x_event) clReleaseEvent(dim_x_event);
    free_texture(queue);
    return GL_INVALID_VALUE;
  }

  err = clEnqueueNDRangeKernel(_queue, _dim_xy_kernel, 2, NULL, wrk_units, NULL, 1, &dim_x_event, &dim_xy_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not enqueue kernel: %d\n", err);
    if(dim_x_event) clReleaseEvent(dim_x_event);
    if(dim_xy_event) clReleaseEvent(dim_xy_event);
    free_texture(queue);
    return GL_INVALID_VALUE;
  }

  err = clWaitForEvents(1, &dim_xy_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not wait for events\n");
    clReleaseEvent(dim_x_event);
    clReleaseEvent(dim_xy_event);
    free_texture(queue);
    return GL_INVALID_VALUE;
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
