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

#define DATA_DENSITY 16

static const char * _path = "common/cl/generator.cl";

static cl_program fetch_program(cl_context context, const char *path) {

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

  printf("cl_err: %d\n", err);

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

  //  printf("gen_net: %d [%d] ~ %d (%d)\n", n, i, x, sizeof(uint32_t) * 4 * DATA_DENSITY * DATA_DENSITY + 1);
  for(x = 0, y = -1, i = 0; i < net_len; x+=n, i+=4) {
    output[i] = (float)(x % wdth);       /* x */
    if(output[i] == 0) y++;
    output[i+1] = (float)y;            /* y */
    output[i+2] = (float)((rand() % 10) << 1);  /* z */
  }

  
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


GLuint generate_texture(const size_t wdth, const size_t hght) {

  cl_int status;
  cl_uint num_platforms;
  status = clGetPlatformIDs(0, NULL, &num_platforms);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "Cannot get the number of OpenCL platforms available.\n");
    exit(EXIT_FAILURE);
  }

  printf("num of platforms: %d\n", (int)num_platforms);
  cl_platform_id platforms[num_platforms];
  status = clGetPlatformIDs(num_platforms, platforms, NULL);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "Cannot get  platform ids.\n");
    exit(EXIT_FAILURE);
  }

  
  cl_uint num_devices;
  status = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, NULL, &num_devices);
  if(status != CL_SUCCESS) {
    fprintf(stderr, "Cannot find any GPU device.\n");
    exit(EXIT_FAILURE);
  }
    
  printf("Num of devices: %d\n", num_devices);

  float *net = generate_net(wdth);
  
  cl_device_id device;
  clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, &device, NULL);

  cl_context_properties properties[] = {
    CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
    CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
    CL_CONTEXT_PLATFORM, (cl_context_properties) platforms[0],
    0};
  
  cl_int err;
  cl_context context;
  context = clCreateContext(properties, 1, &device, NULL, NULL, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create context\n");
  }

  size_t log_size;
  cl_program program = fetch_program(context, _path);
  err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  if(err < 0) {
    /* Find size of log and print to std output */
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
			  0, NULL, &log_size);
    char *program_log = (char*) malloc(log_size + 1);
    program_log[log_size] = '\0';
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
			  log_size + 1, program_log, NULL);
    printf("%s\n", program_log);
    free(program_log);  
  }
    
  cl_kernel kernel;
  kernel = clCreateKernel(program, "gen_texture", &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create kernel\n");
  }

  cl_command_queue queue;
  queue = clCreateCommandQueue(context, device, 0, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create queue\n");
  }

  unsigned int size_r = wdth*hght;
  unsigned int size = size_r*4;
  
  GLuint pixel_buffer;
  cl_mem outbuf = configure_shared_data(context, pixel_buffer, size);   
  clSetKernelArg(kernel, 0, sizeof(cl_mem), &outbuf);

  cl_mem net_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(net), net, &err);
  if( err < 0 ) {
    fprintf(stderr, "Could not create buffer of the net\n");
  }
  
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &net_buffer);
  unsigned int dens_arg = DATA_DENSITY;
  clSetKernelArg(kernel, 2, sizeof(unsigned int), &dens_arg); 

  int flat_lang_size = 4*DATA_DENSITY;
  clSetKernelArg(kernel, 3, flat_lang_size * sizeof(float), NULL);
  
  glFinish();

  err = clEnqueueAcquireGLObjects(queue, 1, &outbuf, 0, NULL, NULL);
  if( err < 0 ) {
    fprintf(stderr, "Could not acquire GL objects\n");
  }
  
  printf("[WxH]%dx%d\n", wdth, hght);
  
  cl_event kernel_event;  
  size_t wrk_units[] = { wdth, hght };  
  
  err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, wrk_units, NULL, 0, NULL, &kernel_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not enqueue kernel\n");    
  }

  err = clWaitForEvents(1, &kernel_event);
  if( err < 0 ) {
    fprintf(stderr, "Could not wait for events\n");
  }

  clEnqueueReleaseGLObjects(queue, 1, &outbuf, 0, NULL, NULL);
  clFinish(queue);
  clReleaseEvent(kernel_event);

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
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
  glGenerateMipmap(GL_TEXTURE_2D);


  clReleaseMemObject(outbuf);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(queue);
  clReleaseProgram(program);
  clReleaseContext(context);

  // Return the ID of the texture we just created
  return textureID;
}


