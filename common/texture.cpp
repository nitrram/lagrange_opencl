#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/freeglut.h>

#include <GLFW/glfw3.h>

#include <time.h>

#define DATA_DENSITY 8

static const char *_path = "common/cl/generator.cl";

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
 
  cl_uint num_devices;
  err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, NULL, &num_devices);
  if(err != CL_SUCCESS) {
    fprintf(stderr, "Cannot find any GPU device.\n");
    exit(EXIT_FAILURE);
  }
    
  printf("Num of devices: %d\n", num_devices);
  
  cl_device_id device;
  err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, &device, NULL);
  if(err < 0) {
    err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_CPU, 1, &device, NULL);
  }

  return device_platform_t{device, platforms[0]};  
}

static cl_program build_program(cl_context context, cl_device_id device, const char *path) {

  FILE* program_handle;
  char *program_buffer, *program_log;
  size_t program_size, log_size;
  cl_int err;
  
  program_handle = fopen(path, "r");
  fseek(program_handle, 0, SEEK_END);
  program_size = ftell(program_handle);
  rewind(program_handle);
  program_buffer = (char *)malloc(program_size + 1);
  program_buffer[program_size] = '\0';
  fread(program_buffer, sizeof(char), program_size, program_handle);
  fclose(program_handle);

  printf("program size: %d\n", program_size);

  cl_program program;
  program = clCreateProgramWithSource(context, 1, (const char **)&program_buffer, &program_size, &err);
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

  free(program_buffer);
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

static cl_mem configure_shared_data(cl_context context, GLuint &pbo, size_t size) {

  int err;

  glGenBuffers(1, &pbo);
  glBindBuffer(GL_ARRAY_BUFFER, pbo);
  glBufferData(GL_ARRAY_BUFFER, size*sizeof(unsigned char), NULL, GL_STATIC_DRAW);
    
  cl_mem result = clCreateFromGLBuffer(context, CL_MEM_WRITE_ONLY, pbo, &err);
  if( err < 0 ){
    fprintf(stderr, "Could not create pixel buffer\n");
  }

  return result;
}

GLuint generate_texture(const uint32_t wdth, const uint32_t hght) {

  cl_int err;

  device_platform_t dev_plat = create_device();

  cl_device_id device = dev_plat.device;  

  cl_context_properties context_properties[] = {
    CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
    CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
    CL_CONTEXT_PLATFORM, (cl_context_properties) dev_plat.platform,
    0};
  
  cl_context context = clCreateContext(context_properties, 1, &device, NULL, NULL, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create context\n");
  }

  float *net = generate_net(wdth);
  // for(int i=0; i < DATA_DENSITY*4*DATA_DENSITY; i+=4) {
  //   printf("%7.1f ", net[i+1]);
  // }
  // printf("\n");

  

  size_t log_size;
  cl_program program = build_program(context, device, _path);  
      
  unsigned int size_r = wdth*hght;
  unsigned int size = size_r*4;

  unsigned int dens_arg = DATA_DENSITY;
  size_t tmp_siz = sizeof(float) * 4 * DATA_DENSITY * wdth;
  float *ftmp = (float*)malloc(tmp_siz);
  cl_mem tmp_buf = clCreateBuffer(context, CL_MEM_READ_WRITE, tmp_siz, ftmp, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create buffer of the tmp_buf\n");
  }

  cl_kernel dim_xy, dim_x;
  dim_x = clCreateKernel(program, "dim_x", &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create kernel dim1\n");
  }

  cl_mem net_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 4*DATA_DENSITY*DATA_DENSITY*sizeof(float), net, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create buffer of the net\n");
  } 
  clSetKernelArg(dim_x, 0, sizeof(cl_mem), &net_buffer);
  clSetKernelArg(dim_x, 1, sizeof(cl_mem), &tmp_buf);
  clSetKernelArg(dim_x, 2, sizeof(unsigned int), &dens_arg);
    
  dim_xy = clCreateKernel(program, "dim_xy", &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create kernel\n");
  }
  
  GLuint pixel_buffer;
  cl_mem outbuf = configure_shared_data(context, pixel_buffer, size);   
  clSetKernelArg(dim_xy, 0, sizeof(cl_mem), &tmp_buf);    
  clSetKernelArg(dim_xy, 1, sizeof(cl_mem), &outbuf);
  clSetKernelArg(dim_xy, 2, sizeof(unsigned int), &dens_arg);  

  
  cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create queue\n");
  }
  printf("tmp_x: %d\n", (int)tmp_siz);

  glFinish();

  err = clEnqueueAcquireGLObjects(queue, 1, &outbuf, 0, NULL, NULL);
  if( err < 0 ) {
    fprintf(stderr, "Could not acquire GL objects\n");
  }
  printf("[WxH]%dx%d\n", wdth, hght);
  
  cl_event dim_xy_event, dim_x_event;  
  size_t wrk_units[] = { wdth, hght };
  const size_t dim = (size_t)wdth;
  
  err = clEnqueueNDRangeKernel(queue, dim_x, 1, NULL, &dim, NULL, 0, NULL, &dim_x_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not enqueue kernel dim1: %d\n", err);    
  }

  float* test = (float*)malloc(tmp_siz);
  printf("testing reading buffer of size: %d\n", tmp_siz);
  err =  clEnqueueReadBuffer(queue, tmp_buf, CL_TRUE, 0, tmp_siz, test, 0, NULL, NULL);
  if(err < 0) {
      fprintf(stderr, "Could not enqueue reading inter: %d\n", err);    
  }
  printf("carry %d\n", clEnqueueBarrier(queue));
  for(int i=0; i< 32; i+=4) {
    printf("%7.1f ", test[i+1]);
  }
  printf("\n");

      
  err = clEnqueueNDRangeKernel(queue, dim_xy, 2, NULL, wrk_units, NULL, 1, &dim_x_event, &dim_xy_event);
  if( err < 0 ) {
     fprintf(stderr, "Could not enqueue kernel: %d\n", err);    
  }
  
  err = clWaitForEvents(1, &dim_xy_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not wait for events\n");
  }
  
  clEnqueueReleaseGLObjects(queue, 1, &outbuf, 0, NULL, NULL);
  clFinish(queue);
  clReleaseEvent(dim_xy_event);
  clReleaseEvent(dim_x_event);


  // printf("hurray\n");
  // for(int i=0; i< 2561; i+=4) {
  //   printf("%7.1f ", pixel_buffer[i+2]);
  // }
  // printf("\n");


  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer);

  // char *data = (char *)malloc(wdth*hght*4);
  // memset(data, 0x90, wdth*hght*3);
  // memset(data+(wdth*hght*3), 0xff, wdth*hght);

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


  clReleaseMemObject(tmp_buf);
  clReleaseMemObject(outbuf);
  clReleaseKernel(dim_xy);
  clReleaseKernel(dim_x);
  clReleaseCommandQueue(queue);
  clReleaseProgram(program);
  clReleaseContext(context);

  // Return the ID of the texture we just created
  return textureID;
}


