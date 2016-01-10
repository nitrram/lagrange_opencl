
COMMON="common"


example:
	g++ -o ogl_cs_example ${COMMON}/shader.cpp ${COMMON}/texture.cpp main.cpp -Iinclude -lGLEW -lGL -lGLU -lglfw -lOpenCL -lm
