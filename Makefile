
COMMON="common"


example:
	g++ -o ogl_cs_example ${COMMON}/shader.cpp ${COMMON}/texture.cpp main.cpp -Iinclude -lEGL -lGLEW -lGL -lGLU -lglfw -lOpenCL -lm -DCL_TARGET_OPENCL_VERSION=120
