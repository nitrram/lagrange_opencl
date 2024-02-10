#include <iostream>
#include <algorithm>
#include <vector>
using namespace std;

#include <GL/glew.h>

#include "shader.h"


// these are Makefile generated
#include "gl_vertex_prog.h"
#include "gl_fragment_prog.h"


GLuint LoadShaders() {

  // Create the shaders
  GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
  GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

  GLint Result = GL_FALSE;
  int InfoLogLength;

  // Compile Vertex Shader
  std::cout << "Compiling vertex shader\n";
  const char *vtx_shader = reinterpret_cast<const char *>(common_gl_vtx_shader);
  glShaderSource(VertexShaderID, 1, &vtx_shader , NULL);
  glCompileShader(VertexShaderID);

  // Check Vertex Shader
  glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if ( InfoLogLength > 0 ){
    std::vector<char> VertexShaderErrorMessage(InfoLogLength+1);
    glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
    std::cerr << &VertexShaderErrorMessage[0] << std::endl;
    return Result;
  }

  // Compile Fragment Shader
  std::cout << "Compiling fragment shader\n";
  const char *frag_shader = reinterpret_cast<const char *>(common_gl_frag_shader);
  glShaderSource(FragmentShaderID, 1, &frag_shader, NULL);
  glCompileShader(FragmentShaderID);

  // Check Fragment Shader
  glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if ( InfoLogLength > 0 ){
    std::vector<char> FragmentShaderErrorMessage(InfoLogLength+1);
    glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
    std::cerr << &FragmentShaderErrorMessage[0] << std::endl;
    return Result;
  }

  // Link the program
  std::cout << "Linking program\n";
  GLuint ProgramID = glCreateProgram();
  glAttachShader(ProgramID, VertexShaderID);
  glAttachShader(ProgramID, FragmentShaderID);
  glLinkProgram(ProgramID);

  // Check the program
  glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
  glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if ( InfoLogLength > 0 ){
    std::vector<char> ProgramErrorMessage(InfoLogLength+1);
    glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
    std::cerr << &ProgramErrorMessage[0] << std::endl;
    return Result;
  }

  glDeleteShader(VertexShaderID);
  glDeleteShader(FragmentShaderID);

  return ProgramID;
}
