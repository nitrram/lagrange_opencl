// Include standard headers
#include <stdio.h>
#include <stdlib.h>

#include <CL/cl.h>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include "common/shader.hpp"
#include "common/texture.hpp"

#define WIDTH 1024
#define HEIGHT 768

int main( void ) {
  // Initialise GLFW
  if( !glfwInit() )
    {
      fprintf( stderr, "Failed to initialize GLFW\n" );
      return -1;
    }
    
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Open a window and create its OpenGL context
  window = glfwCreateWindow( WIDTH, HEIGHT, "Langrange", NULL, NULL);
  if( window == NULL ){
    fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);

  // Initialize GLEW
  glewExperimental = true; // Needed for core profile
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    return -1;
  }

  // Ensure we can capture the escape key being pressed below
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

  GLuint VertexArrayID;
  glGenVertexArrays(1, &VertexArrayID);
  glBindVertexArray(VertexArrayID);

  // Create and compile our GLSL program from the shaders
  GLuint programID = LoadShaders( "TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader" );  

  // Get a handle for our "myTextureSampler" uniform
  GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");

  // Our vertices. Tree consecutive floats give a 3D vertex; Three consecutive vertices give a triangle.
  // A cube has 6 faces with 2 triangles each, so this makes 6*2=12 triangles, and 12*3 vertices
  static const GLfloat g_vertex_buffer_data[] = {
    -1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f,
     1.0f, -1.0f,
  };

  GLfloat tex_coords[] = {1.0f, 0.0f,
			  0.0f, 1.0f,
			  1.0f, 1.0f,
			  0.0f, 0.0f};

  GLuint vertexbuffer[2];
  glGenBuffers(2, vertexbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer[0]);
  glBufferData(GL_ARRAY_BUFFER, 8*sizeof(GLfloat), g_vertex_buffer_data, GL_STATIC_DRAW);

  // 1rst attribute buffer : vertices
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
			0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			2,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer[1]);
  glBufferData(GL_ARRAY_BUFFER, 8*sizeof(GLfloat), tex_coords, GL_STATIC_DRAW);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

  
  GLuint Texture = generate_texture(800,700);

  // Use our shader
  glUseProgram(programID);

  // Draw the triangle !
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // 2*3 indices starting at 0 -> 2 triangles

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  // Swap buffers
  glfwSwapBuffers(window);

  /* main loop */
  do{
    glfwPollEvents();
  } // Check if the ESC key was pressed or the window was closed
  while( ((glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS) &&
	  (glfwGetKey(window, GLFW_KEY_Q) != GLFW_PRESS)) &&
	 glfwWindowShouldClose(window) == 0 );

  // Cleanup VBO and shader
  glDeleteBuffers(2, vertexbuffer);
  glDeleteProgram(programID);
  glDeleteTextures(1, &TextureID);
  glDeleteVertexArrays(1, &VertexArrayID);

  // Close OpenGL window and terminate GLFW
  glfwTerminate();

  return 0;
}
