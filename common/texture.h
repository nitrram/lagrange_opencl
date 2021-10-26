#pragma once

enum e_release { all, program, context, queue, net, dim_x, pixel, dim_x_kernel, dim_xy_kernel, dim_xy_event, dim_x_event };

GLuint generate_texture(const uint32_t wdth, const uint32_t hght);

void update_texture();

void free_texture(enum e_release);
