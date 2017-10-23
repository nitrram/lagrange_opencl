
COMMON="common"

ifdef __APPLE__
EXTRA_LD_FLAGS=-framework OpenGL -lglfw -lGLEW -framework OpenCL
else
EXTRA_LD_FLAGS=-lGLEW -lGL -lGLU -lglfw -lOpenCL -lm
endif


example:
	g++ -std=c++11 -o ogl_cs_example ${COMMON}/shader.cpp ${COMMON}/texture.cpp main.cpp -Iinclude ${EXTRA_LD_FLAGS}
