#pragma once

#include <cstdint>

#if defined(__APPLE__) || defined(MACOSX)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

GLuint generate_texture(const uint32_t wdth, const uint32_t hght);

void update_texture();

void free_texture();

