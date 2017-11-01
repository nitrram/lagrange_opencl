/* __constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST; */

float langrage_y(float var, __global float4 *ar, int density) {

  int i,j;
  float li = 1;
  float ln = 0;

  for(i=0; i<density; i++){
    for(j=0; j<density; j++){
      if(i!=j){
	li *= (var - ar[j].y) / (ar[i].y - ar[j].y);
      }
    }
    ln += li*ar[i].z;
    li = 1;
  }

  return ln;  
}


/* 0 0 0; 255 0 0; 255 255 0; 255 255 255*/

uchar4 dist_colors(int z) {
  char r = 0, g = 0, b=0;
  //float c = 12.5f; /*coefficient belonging to a colour 10 / 4*/
//  int z1 = z + 12;

  char m = 0xff;

  int cond = z > 75;

  r = (z & ~m) | (-(z > 75) & m);
  g = (z & ~m) | (-(z > 12) & m);
  b = (z & ~m) | (-(z < 75) & m);

 /*
  if(z1 < c){
    b=(char)z1;
  }
  else if ((z1 >= c) && (z1 < 3*c)){    
    g = 255;
    b=255-((char)z1-c);
  }
  else if ((z1 >= 3*c) && (z1 < 5*c)){
    b = 0;
    g = 255;
    r=((char)z1-3*c);
  }
  else if(z1 >= 5*c){
    b = 0;
    g = 255;
    r = 255;  
    g-=((char)z1-5*c);
  } 
  */

  return ((uchar4)(r, g, b, 0) - (uchar4)(z*0.64, z*0.85, z*0.41, 0));
}

float langrage_x(float var, __global float4 *ar, int density) {

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
		    __global float4 *dst_buf,
		    int dens) {
  
  int x = get_global_id(0);
  
  int siz = get_global_size(0);
  
  int offset = x * dens;
  for(int idx=0; idx<dens; idx++) {
    dst_buf[offset+idx] = (float4)((float)x,
				   src_buf[idx*dens].y,
				   langrage_x((float)x, src_buf+(idx*dens), dens),
				   0.0);
  }
}

__kernel void dim_xy(__global float4 *lang_x_buf,
		     __global uchar4 *dst_buf,
		     int dens) {

  int x = get_global_id(0);
  int y = get_global_id(1);

  int siz = get_global_size(0);
    
  int z = (int)langrage_y(y, lang_x_buf+(x*dens), dens);
       
  /* z ~ [R|G|B] */

  int oi = x + (y*siz);

  //  int r = (y%8 && x%8) ? 0 : 120;
  
  dst_buf[oi] = dist_colors(z); //(uchar4)(0,0,r,0);
}

