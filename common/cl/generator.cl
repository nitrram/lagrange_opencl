/* __constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST; */

float lagrange_y(float var, __global float2 *ar, int density) {

  int i,j;
  float li = 1;
  float ln = 0;

  for(i=0; i<density; i++){
    for(j=0; j<density; j++){
      if(i!=j){
        li *= (var - ar[j].x) / (ar[i].x - ar[j].x);
      }
    }
    ln += li*ar[i].y; 
    li = 1;
  }

  return ln;
}


/* 0 0 0; 255 0 0; 255 255 0; 255 255 255*/

uchar4 dist_colors(int z) {

	float z2 = (z != 0) ? (fmin(sign((float)z) * log(fabs((float)z)), 5.94f)) : 0.0f;

  char zr = 255.0f * exp(-pow((float)z2 + 5.94f, 2.0f) / 5.0f);
  char zg = 255.0f * exp(-pow((float)z2 + 0.0f, 2.0f) / 10.0f);
	char zb = 255.0f * exp(-pow((float)z2 - 5.94f, 2.0f) / 5.0f);

	return (uchar4)(zr, zg, zb, 0);
}

float lagrange_x(float var, __global float4 *ar, int density) {

  int i,j;
  float li = 1;
  float ln = 0;

  for(i=0; i<density; i++){
    for(j=0; j<density; j++){
      if(i!=j){
        li *= (var - ar[j].x) / (ar[i].x - ar[j].x);
      }
    }
    ln += li*ar[i].z;
    li = 1;
  }

  return ln;
}



__kernel void dim_x(__global float4 *src_buf,
                    __global float2 *dst_buf) {

	size_t g = get_group_id(0);
	size_t dens = get_local_size(0);
	size_t idx = get_local_id(0);

	size_t x = g+idx;
	size_t offset = g * dens;
	size_t src_offset = idx * dens;

	//	dens-tuplets to iterate through in the next step
	dst_buf[idx+offset] = (float2)(src_buf[src_offset].y,
																 lagrange_x((float)x, src_buf+(src_offset), dens));

}

__kernel void dim_xy(__global float2 *lagr_x_buf,
                     __global uchar4 *dst_buf,
                     int dens) {

  int x = get_global_id(0);
  int y = get_global_id(1);

  int stride_x = get_global_size(0);

  int z = (int)lagrange_y(y, lagr_x_buf+(x*dens), dens);

  // z ~ [R|G|B]

  int oi = x + (stride_x*y);

  dst_buf[oi] = dist_colors(z); //(uchar4)(0,0,r,0);
}
