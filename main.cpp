//----------------------------------------------------------------------------
//
// "Creature Box" -- flocking app
//
// by Christopher Rasmussen, cer@cis.udel.edu
//
// 1.0: initial version, March, 2014
// 1.1: updated for OpenGL 3.3, March 2016
// 1.2: framerate limiting, corrected local axis scaling, "personality" variation in flockers, March 2017
// 1.3: integration with Bullet physics library and .obj loading
//
//----------------------------------------------------------------------------

// Include standard headers

#include <stdio.h>
#include <stdlib.h>

// Include GLEW

#include <GL/glew.h>

// Include GLFW

#include <GLFW/glfw3.h>

// Include GLM

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

// Bullet-specific stuff

#include "Bullet_Utils.hh"

// for loading GLSL shaders

#include <common/shader.hpp>

// creature-specific stuff

#include "Flocker.hh"
#include "Predator.hh"

//----------------------------------------------------------------------------

// to avoid gimbal lock issues...

#define MAX_LATITUDE_DEGS     89.0
#define MIN_LATITUDE_DEGS    -89.0

#define CAMERA_MODE_ORBIT    1
#define CAMERA_MODE_CREATURE 2

#define MIN_ORBIT_CAM_RADIUS    (7.0)
#define MAX_ORBIT_CAM_RADIUS    (25.0)

#define DEFAULT_ORBIT_CAM_RADIUS            22.5
#define DEFAULT_ORBIT_CAM_LATITUDE_DEGS     0.0
#define DEFAULT_ORBIT_CAM_LONGITUDE_DEGS    90.0

//----------------------------------------------------------------------------

// some convenient globals 

GLFWwindow* window;

GLuint programID;
GLuint objprogramID;

GLuint MatrixID;
GLuint ViewMatrixID;
GLuint ModelMatrixID;

GLuint objMatrixID;
GLuint objViewMatrixID;
GLuint objModelMatrixID;
GLuint objTextureID;
GLuint objLightID;

GLuint VertexArrayID;

float box_width  = 9.0;
float box_height = 5.0;
float box_depth =  7.0;

// used for obj files

vector<glm::vec3> obj_vertices;
vector<glm::vec2> obj_uvs;
vector<glm::vec3> obj_normals;
GLuint obj_Texture;
GLuint pred_Texture;
vector<unsigned short> obj_indices;
vector<glm::vec3> obj_indexed_vertices;
vector<glm::vec2> obj_indexed_uvs;
vector<glm::vec3> obj_indexed_normals;
GLuint obj_vertexbuffer;
GLuint obj_uvbuffer;
GLuint obj_normalbuffer;
GLuint obj_elementbuffer;

// these along with Model matrix make MVP transform

glm::mat4 ProjectionMat;
glm::mat4 ViewMat;

// sim-related globals

double target_FPS = 60.0;

bool using_obj_program = false;

bool is_paused = false;
bool is_physics_active = false;

double orbit_cam_radius = DEFAULT_ORBIT_CAM_RADIUS;
double orbit_cam_delta_radius = 0.1;

double orbit_cam_latitude_degs = DEFAULT_ORBIT_CAM_LATITUDE_DEGS;
double orbit_cam_longitude_degs = DEFAULT_ORBIT_CAM_LONGITUDE_DEGS;
double orbit_cam_delta_theta_degs = 1.0;

int win_scale_factor = 60;
int win_w = win_scale_factor * 16.0;
int win_h = win_scale_factor * 9.0;
 
int camera_mode = CAMERA_MODE_ORBIT;

int num_flockers = 50;   // 400 is "comfortable" max on my machine
int num_predators = 1;

extern int flocker_history_length;
extern int flocker_draw_mode;
extern vector <Flocker *> flocker_array;
extern vector <Predator *> predator_array;
extern vector <vector <double> > flocker_squared_distance;
extern vector <vector <double> > p_to_f_squared_distance;

GLuint box_vertexbuffer;
GLuint box_colorbuffer;

//----------------------------------------------------------------------------

void end_program()
{
  // Cleanup VBOs and shader

  glDeleteBuffers(1, &box_vertexbuffer);
  glDeleteBuffers(1, &box_colorbuffer);
  glDeleteProgram(programID);
  glDeleteProgram(objprogramID);
  glDeleteVertexArrays(1, &VertexArrayID);
  
  // Close OpenGL window and terminate GLFW

  glfwTerminate();

  exit(1);
}

//----------------------------------------------------------------------------

// corner "origin" is at (0, 0, 0) -- must translate to center

void draw_box(glm::mat4 Model)
{
  // Our ModelViewProjection : multiplication of our 3 matrices

  glm::mat4 MVP = ProjectionMat * ViewMat * Model;

  // make this transform available to shaders  

  glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

  // 1st attribute buffer : vertices

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, box_vertexbuffer);
  glVertexAttribPointer(0,                  // attribute. 0 to match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);
  
  // 2nd attribute buffer : colors

  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, box_colorbuffer);
  glVertexAttribPointer(1,                                // attribute. 1 to match the layout in the shader.
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
			);

  // Draw the box!

  glDrawArrays(GL_LINES, 0, 24); 
  
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

//----------------------------------------------------------------------------

// handle key presses

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  // quit

  if (key == GLFW_KEY_Q && action == GLFW_PRESS)
    end_program();

  // pause

  else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    is_paused = !is_paused;


  // toggle physics/flocking dynamics modes
  
  else if (key == GLFW_KEY_P && action == GLFW_PRESS) {
    is_physics_active = !is_physics_active;

    // spin up physics simulator
    
    if (is_physics_active) {

      copy_flocker_states_to_graphics_objects();
      initialize_bullet_simulator();

    }

    // stop physics simulator
    
    else {

      delete_bullet_simulator();

    }
  }
  
  // orbit rotate

  else if (key == GLFW_KEY_A && (action == GLFW_PRESS || action == GLFW_REPEAT)) 
    orbit_cam_longitude_degs -= orbit_cam_delta_theta_degs;
  else if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT))
    orbit_cam_longitude_degs += orbit_cam_delta_theta_degs;
  else if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    if (orbit_cam_latitude_degs + orbit_cam_delta_theta_degs <= MAX_LATITUDE_DEGS)
      orbit_cam_latitude_degs += orbit_cam_delta_theta_degs;
  }
  else if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    if (orbit_cam_latitude_degs - orbit_cam_delta_theta_degs >= MIN_LATITUDE_DEGS)
      orbit_cam_latitude_degs -= orbit_cam_delta_theta_degs;
  }

  // orbit zoom in/out

  else if (key == GLFW_KEY_Z && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    if (orbit_cam_radius + orbit_cam_delta_radius <= MAX_ORBIT_CAM_RADIUS)
      orbit_cam_radius += orbit_cam_delta_radius;
  }
  else if (key == GLFW_KEY_C && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    if (orbit_cam_radius - orbit_cam_delta_radius >= MIN_ORBIT_CAM_RADIUS)
      orbit_cam_radius -= orbit_cam_delta_radius;
  }

  // orbit pose reset

  else if (key == GLFW_KEY_X && action == GLFW_PRESS) {
    orbit_cam_radius = DEFAULT_ORBIT_CAM_RADIUS;
    orbit_cam_latitude_degs = DEFAULT_ORBIT_CAM_LATITUDE_DEGS;
    orbit_cam_longitude_degs = DEFAULT_ORBIT_CAM_LONGITUDE_DEGS;
  }

  // flocker drawing options
  // Draws Arrow
  else if (key == GLFW_KEY_7 && action == GLFW_PRESS) {
    flocker_draw_mode = DRAW_MODE_OBJ;
    using_obj_program = true;
  }
  else if (key == GLFW_KEY_8 && action == GLFW_PRESS) {
    flocker_draw_mode = DRAW_MODE_POLY;
    using_obj_program = false;
  }
  else if (key == GLFW_KEY_9 && action == GLFW_PRESS) {
    flocker_draw_mode = DRAW_MODE_AXES;
    using_obj_program = false;
  }
  else if (key == GLFW_KEY_0 && action == GLFW_PRESS) {
    flocker_draw_mode = DRAW_MODE_HISTORY;
    using_obj_program = false;
  }
}

//----------------------------------------------------------------------------

// allocate simulation data structures and populate them

void initialize_flocking_simulation()
{
  // box geometry with corner at origin

  static const GLfloat box_vertex_buffer_data[] = {
    0.0f, 0.0f, 0.0f,                      // X axis
    box_width, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,                      // Y axis
    0.0f, box_height, 0.0f,
    0.0f, 0.0f, 0.0f,                      // Z axis
    0.0f, 0.0f, box_depth,
    box_width, box_height, box_depth,      // other edges
    0.0f, box_height, box_depth,
    box_width, box_height, 0.0f,
    0.0f, box_height, 0.0f,
    box_width, 0.0f, box_depth,
    0.0f, 0.0f, box_depth,
    box_width, box_height, box_depth,
    box_width, 0.0f, box_depth,
    0.0f, box_height, box_depth,
    0.0f, 0.0f, box_depth,
    box_width, box_height, 0.0f,
    box_width, 0.0f, 0.0f,
    box_width, box_height, box_depth,
    box_width, box_height, 0.0f,
    0.0f, box_height, box_depth,
    0.0f, box_height, 0.0f,
    box_width, 0.0f, box_depth,
    box_width, 0.0f, 0.0f,
  };

  glGenBuffers(1, &box_vertexbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, box_vertexbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(box_vertex_buffer_data), box_vertex_buffer_data, GL_STATIC_DRAW);

  // "axis" edges are colored, rest are gray

  static const GLfloat box_color_buffer_data[] = { 
    1.0f, 0.0f, 0.0f,       // X axis is red 
    1.0f, 0.0f, 0.0f,        
    0.0f, 1.0f, 0.0f,       // Y axis is green 
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,       // Z axis is blue
    0.0f, 0.0f, 1.0f,
    0.5f, 0.5f, 0.5f,       // all other edges gray
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
  };

  glGenBuffers(1, &box_colorbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, box_colorbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(box_color_buffer_data), box_color_buffer_data, GL_STATIC_DRAW);
  
  // the simulation proper

  //  initialize_random();

  flocker_squared_distance.resize(num_flockers);
  flocker_array.clear();
  p_to_f_squared_distance.resize(num_predators);
  predator_array.clear();

  for (int i = 0; i < num_flockers; i++) {
    flocker_array.push_back(new Flocker(i, 
					  uniform_random(0, box_width), uniform_random(0, box_height), uniform_random(0, box_depth),
					  uniform_random(-0.01, 0.01), uniform_random(-0.01, 0.01), uniform_random(-0.01, 0.01),
					  0.002,            // randomness
					  0.05, 0.5, uniform_random(0.01, 0.03),  // min, max separation distance, weight
					  0.5,  1.0, uniform_random(0.0005, 0.002), // min, max alignment distance, weight
					  1.0,  1.5, uniform_random(0.0005, 0.002), // min, max cohesion distance, weight
                      0.0,  1.0, uniform_random(0.0005, 0.4), // min, max fear distance, weight
					//					  0.05, 0.5, 0.02,  // min, max separation distance, weight
					//					  0.5,  1.0, 0.001, // min, max alignment distance, weight
					//					  1.0,  1.5, 0.001, // min, max cohesion distance, weight
					  1.0,  1.0, 1.0,
					  flocker_history_length));

    flocker_squared_distance[i].resize(num_flockers);
    
  }
  for (int i = 0; i < num_predators; i++) {
    predator_array.push_back(new Predator(i,
					  uniform_random(0, box_width), uniform_random(0, box_height), uniform_random(0, box_depth),
					  uniform_random(-0.01, 0.01), uniform_random(-0.01, 0.01), uniform_random(-0.01, 0.01),
                      0.1,  1.5, uniform_random(0.005, 0.02), // min, max hunger distance, weight
					  1.0,  1.0, 1.0,
					  flocker_history_length));

    p_to_f_squared_distance[i].resize(num_flockers);
  }
}

//----------------------------------------------------------------------------

// move creatures around -- no drawing

void update_flocking_simulation()
{
  int i;

  // precalculate inter-flocker distances

  calculate_flocker_squared_distances();
  calculate_p_to_f_squared_distances();

  // get new_position, new_velocity for each flocker

  for (i = 0; i < flocker_array.size(); i++) 
    flocker_array[i]->update();
    
  for (i = 0; i < predator_array.size(); i++)
    predator_array[i]->update();

  // handle wrapping and make new position, velocity into current

  for (i = 0; i < flocker_array.size(); i++) 
    flocker_array[i]->finalize_update(box_width, box_height, box_depth);
    
  for (i = 0; i < predator_array.size(); i++)
    predator_array[i]->finalize_update(box_width, box_height, box_depth);
}

//----------------------------------------------------------------------------

// place the camera here

void setup_camera()
{
  ProjectionMat = glm::perspective(50.0f, (float) win_w / (float) win_h, 0.1f, 35.0f);

  if (camera_mode == CAMERA_MODE_ORBIT) {
    
    double orbit_cam_azimuth = glm::radians(orbit_cam_longitude_degs);
    double orbit_cam_inclination = glm::radians(90.0 - orbit_cam_latitude_degs);
    
    double x_cam = orbit_cam_radius * sin(orbit_cam_inclination) * cos(orbit_cam_azimuth); // 0.5 * box_width;
    double z_cam = orbit_cam_radius * sin(orbit_cam_inclination) * sin(orbit_cam_azimuth); // 0.5 * box_height;
    double y_cam = orbit_cam_radius * cos(orbit_cam_inclination); // 15.0;
    
    ViewMat = glm::lookAt(glm::vec3(x_cam, y_cam, z_cam),   // Camera location in World Space
			  glm::vec3(0,0,0),                 // and looks at the origin
			  glm::vec3(0,-1,0)                  // Head is up (set to 0,-1,0 to look upside-down)
			  );
  }
  else {
    
    printf("only orbit camera mode currently supported\n");
    exit(1);
    
  }
}

//----------------------------------------------------------------------------

// no error-checking -- you have been warned...

void load_objects_and_textures(int argc, char **argv)
{
  if (argc == 1) {
    loadOBJ("cube.obj", obj_vertices, obj_uvs, obj_normals);
    obj_Texture = loadDDS("uvmap.DDS");
    pred_Texture = loadDDS("inverse.DDS");
  }
  else if (!strcmp("cube", argv[1])) {
    loadOBJ("cube.obj", obj_vertices, obj_uvs, obj_normals);
    obj_Texture = loadDDS("uvmap.DDS");
    pred_Texture = loadDDS("inverse.DDS");
  }
  else if (!strcmp("suzanne", argv[1])) {
    loadOBJ("suzanne.obj", obj_vertices, obj_uvs, obj_normals);
    obj_Texture = loadDDS("uvmap.DDS");
    pred_Texture = loadDDS("inverse.DDS");
  }
  else if (!strcmp("banana", argv[1])) {
    loadOBJ("banana.obj", obj_vertices, obj_uvs, obj_normals);
    obj_Texture = loadBMP_custom("banana.bmp");
    pred_Texture = loadDDS("inverse.DDS");
  }
  else {
    printf("unsupported object\n");
    exit(1);
  }

  // if not doing bullet demo, scale down objects to creature size -- kind of eye-balled this

  if (argc < 3) {
    for (int i = 0; i < obj_vertices.size(); i++) {
      obj_vertices[i].x *= 0.15;
      obj_vertices[i].y *= 0.15;
      obj_vertices[i].z *= 0.15;
    }
  }

  indexVBO(obj_vertices, obj_uvs, obj_normals, obj_indices, obj_indexed_vertices, obj_indexed_uvs, obj_indexed_normals);

  // Load into array buffers
  
  glGenBuffers(1, &obj_vertexbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, obj_vertexbuffer);
  glBufferData(GL_ARRAY_BUFFER, obj_indexed_vertices.size() * sizeof(glm::vec3), &obj_indexed_vertices[0], GL_STATIC_DRAW);
  
  glGenBuffers(1, &obj_uvbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, obj_uvbuffer);
  glBufferData(GL_ARRAY_BUFFER, obj_indexed_uvs.size() * sizeof(glm::vec2), &obj_indexed_uvs[0], GL_STATIC_DRAW);
  
  glGenBuffers(1, &obj_normalbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, obj_normalbuffer);
  glBufferData(GL_ARRAY_BUFFER, obj_indexed_normals.size() * sizeof(glm::vec3), &obj_indexed_normals[0], GL_STATIC_DRAW);
  
  // Generate a buffer for the indices as well
  glGenBuffers(1, &obj_elementbuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj_elementbuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, obj_indices.size() * sizeof(unsigned short), &obj_indices[0] , GL_STATIC_DRAW);
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

int main(int argc, char **argv)
{  
  // Initialise GLFW

  if( !glfwInit() )
    {
      fprintf( stderr, "Failed to initialize GLFW\n" );
      getchar();
      return -1;
    }
  
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //We don't want the old OpenGL 
  
  // Open a window and create its OpenGL context

  window = glfwCreateWindow(win_w, win_h, "creatures", NULL, NULL);
  if( window == NULL ){
    fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
    getchar();
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  
  // Initialize GLEW

  glewExperimental = true; // Needed for core profile
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    getchar();
    glfwTerminate();
    return -1;
  }

  // background color, depth testing

  glClearColor(0.0f, 0.0f, 0.1f, 0.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS); 
  //  glEnable(GL_CULL_FACE);  // we don't want this enabled when drawing the creatures as floating triangles

  // vertex arrays
  
  glGenVertexArrays(1, &VertexArrayID);
  glBindVertexArray(VertexArrayID);

  // Create and compile our GLSL program from the shaders

  // obj rendering mode uses shaders that came with bullet demo program

  objprogramID = LoadShaders( "StandardShading.vertexshader", "StandardShading.fragmentshader" );

  // all other rendering modes

  programID = LoadShaders( "Creatures.vertexshader", "Creatures.fragmentshader" );

  // Get handles for our uniform variables

  MatrixID = glGetUniformLocation(programID, "MVP");
  ViewMatrixID = glGetUniformLocation(programID, "V");
  ModelMatrixID = glGetUniformLocation(programID, "M");

  objMatrixID = glGetUniformLocation(objprogramID, "MVP");
  objViewMatrixID = glGetUniformLocation(objprogramID, "V");
  objModelMatrixID = glGetUniformLocation(objprogramID, "M");

  objTextureID  = glGetUniformLocation(objprogramID, "myTextureSampler");
  objLightID = glGetUniformLocation(objprogramID, "LightPosition_worldspace");

  // Use our shader

  glUseProgram(programID);

  // register all callbacks
  
  glfwSetKeyCallback(window, key_callback);

  // objects, textures
  
  load_objects_and_textures(argc, argv);

  // simulation

  initialize_random();
  initialize_flocking_simulation();

  // run a whole different program if bullet demo option selected
  
  if (argc == 3) { 
    bullet_hello_main(argc, argv);
    return 1;
  }
  
  // timing stuff

  double currentTime, lastTime;
  double target_period = 1.0 / target_FPS;
  
  lastTime = glfwGetTime();

  // enter simulate-render loop (with event handling)
  
  do {

    // STEP THE SIMULATION -- EITHER FLOCKING OR PHYSICS

    if (!is_paused) {
      if (!is_physics_active)
	update_flocking_simulation();
      else
	update_physics_simulation(target_period);

    }
    
    // RENDER IT

    // Clear the screen

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
    // set model transform and color for each triangle and draw it offscreen

    setup_camera();

    // a centering translation for viewing

    glm::mat4 M = glm::translate(glm::vec3(-0.5f * box_width, -0.5f * box_height, -0.5f * box_depth));

    // draw box and creatures

    glUseProgram(programID);
    draw_box(M);

    if (using_obj_program)
      glUseProgram(objprogramID);
    else
      glUseProgram(programID);

    for (int i = 0; i < flocker_array.size(); i++) 
      flocker_array[i]->draw(M);
      
    for (int i = 0; i < predator_array.size(); i++)
      predator_array[i]->draw(M);

    // busy wait if we are going too fast

    do {
      currentTime = glfwGetTime();
    } while (currentTime - lastTime < target_period);

    lastTime = currentTime;

    // Swap buffers

    glfwSwapBuffers(window);
    glfwPollEvents();

  } while ( glfwWindowShouldClose(window) == 0 );
  
  end_program();
  
  return 0;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
