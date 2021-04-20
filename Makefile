
#COMMON="common"

CXX := g++
CXXFLAGS += -Wall -fPIE

CC := gcc
CFLAGS += -pedantic -Iinclude -DCL_TARGET_OPENCL_VERSION=120

# generating header out of opencl code
HEX := xxd
HEADER_PREFIX := /* This is an automaticaly generated header consisting of hexadecimal ANSI C strings\n * based on $(notdir $(cl_SOURCES))\n */\n\n

LIBS := EGL GLEW GL GLU glfw OpenCL

DIRS := . common

OBJDIR := obj

C_SOURCES := $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
CXX_SOURCES := $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cpp))
CL_SOURCES := $(wildcard common/cl/*.cl)

C_OBJECTS := $(patsubst %.c,%.o,$(C_SOURCES))
CPP_OBJECTS := $(patsubst %.cpp,%.o,$(CXX_SOURCES))

OBJECTS += $(CPP_OBJECTS)
OBJECTS += $(C_OBJECTS)


.SUFFIXES:
.SUFFIXES: .o .c .cpp

.cpp.o:
	@echo "cpp compilation"
	$(CXX) $(addprefix -l,$(LIBS)) $(CXXFLAGS) -c $< -o $@

.c.o:
	@echo "cc compilation"
	$(CC) $(addprefix -l,$(LIBS)) $(CFLAGS) -c $< -o $@


all: example


opencl_program_source: $(CL_SOURCES)
	cat $^ > $@

common/cl_prog.h: opencl_program_source
	$(HEX) -i $< > $@

cl_header: common/cl_prog.h
	sed -i "1 s|^|$(HEADER_PREFIX)|" $<
	@rm -f cl_program

example: | cl_header $(OBJECTS)
	@echo "\033[0;32mcompilation example\033[0;0m"
	@echo $(OBJECTS)
	$(CXX) $(addprefix -l,$(LIBS)) $(CFLAGS) $(OBJECTS) -g -o $@


###g++ -o $* ${COMMON}/shader.cpp ${COMMON}/texture.c main.c -Iinclude -lEGL -lGLEW -lGL -lGLU -lglfw -lOpenCL -lm -DCL_TARGET_OPENCL_VERSION=120



.PHONY: clean
clean: 
	@echo ./*.o ./example **/*.o ./common/cl_prog.h
	@rm -rf ./*.o ./example **/*.o ./common/cl_prog.h


