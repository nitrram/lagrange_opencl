#pragma once

#if defined(__APPLE__) || defined(MACOSX)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h
#endif

GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path);


