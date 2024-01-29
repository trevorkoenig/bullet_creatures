//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// "Creature Box" -- flocking app
//
// by Christopher Rasmussen
//
// CISC 440/640, March, 2014
// updated to OpenGL 3.3, March, 2016
//
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

#include "Predator.hh"

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

vector <Predator *> predator_array;

extern int flocker_history_length;
extern int flocker_draw_mode;

extern glm::mat4 ViewMat;
extern glm::mat4 ProjectionMat;

extern GLuint ModelMatrixID;
extern GLuint ViewMatrixID;
extern GLuint MatrixID;

extern GLuint objModelMatrixID;
extern GLuint objViewMatrixID;
extern GLuint objMatrixID;
extern GLuint objLightID;
extern GLuint objTextureID;

extern GLuint pred_Texture;
extern GLuint obj_vertexbuffer;
extern GLuint obj_uvbuffer;
extern GLuint obj_normalbuffer;
extern GLuint obj_elementbuffer;
extern vector<unsigned short> obj_indices;

vector <vector <double> > p_to_f_squared_distance;

extern vector <Flocker *> flocker_array;

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

Predator::Predator(int _index,
    double init_x, double init_y, double init_z,
    double init_vx, double init_vy, double init_vz,
    double _min_hunger_distance, double _max_hunger_distance,  double _hunger_weight,
    float r, float g, float b,
    int max_hist) : Creature(_index, init_x, init_y, init_z, init_vx, init_vy, init_vz, r, g, b, max_hist)
{
    min_hunger_distance = _min_hunger_distance;
    min_squared_hunger_distance = min_hunger_distance * min_hunger_distance;
    
    max_hunger_distance = _max_hunger_distance;
    max_squared_hunger_distance = max_hunger_distance * max_hunger_distance;
    
    hunger_weight = _hunger_weight;
    
    inv_range_squared_hunger_distance = 1.0 / (max_squared_hunger_distance - min_squared_hunger_distance);

}

//----------------------------------------------------------------------------

void Predator::draw(glm::mat4 Model)
{
  if (flocker_draw_mode == DRAW_MODE_OBJ) {

    // set light position
    
    glm::vec3 lightPos = glm::vec3(4,4,4);
    glUniform3f(objLightID, lightPos.x, lightPos.y, lightPos.z);

    glm::mat4 RotationMatrix = glm::mat4(); // identity    -- glm::toMat4(quat_orientations[i]);
    glm::mat4 TranslationMatrix = translate(glm::mat4(), glm::vec3(position.x, position.y, position.z));
    glm::mat4 ModelMatrix = TranslationMatrix * RotationMatrix;

    glm::mat4 MVP = ProjectionMat * ViewMat * Model * ModelMatrix;

    // Send our transformation to the currently bound shader,
    // in the "MVP" uniform
    glUniformMatrix4fv(objMatrixID, 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(objModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
    glUniformMatrix4fv(objViewMatrixID, 1, GL_FALSE, &ViewMat[0][0]);

    // Bind this object's texture in Texture Unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pred_Texture);
    // Set our "myTextureSampler" sampler to use Texture Unit 0
    glUniform1i(objTextureID, 0);

    // 1st attribute buffer : vertices
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, obj_vertexbuffer);
    glVertexAttribPointer(
			  0,                  // attribute
			  3,                  // size
			  GL_FLOAT,           // type
			  GL_FALSE,           // normalized?
			  0,                  // stride
			  (void*)0            // array buffer offset
			  );
    
    // 2nd attribute buffer : UVs
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, obj_uvbuffer);
    glVertexAttribPointer(
			  1,                                // attribute
			  2,                                // size
			  GL_FLOAT,                         // type
			  GL_FALSE,                         // normalized?
			  0,                                // stride
			  (void*)0                          // array buffer offset
			  );
    
    // 3rd attribute buffer : normals
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, obj_normalbuffer);
    glVertexAttribPointer(
			  2,                                // attribute
			  3,                                // size
			  GL_FLOAT,                         // type
			  GL_FALSE,                         // normalized?
			  0,                                // stride
			  (void*)0                          // array buffer offset
			  );
      
    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj_elementbuffer);

    // Draw the triangles !
    glDrawElements(
		   GL_TRIANGLES,      // mode
		   obj_indices.size(),    // count
		   GL_UNSIGNED_SHORT,   // type
		   (void*)0           // element array buffer offset
		   );
      

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    return;
  }


  int draw_mode, num_vertices;
  
  GLfloat *vertex_buffer_data;
  GLfloat *color_buffer_data;
   
  // position "trail" using history

  if (flocker_draw_mode == DRAW_MODE_HISTORY) {

    num_vertices = position_history.size();
    draw_mode = GL_POINTS;

    vertex_buffer_data = (GLfloat *) malloc(3 * num_vertices * sizeof(GLfloat));
    color_buffer_data = (GLfloat *) malloc(3 * num_vertices * sizeof(GLfloat));

    glPointSize(5);

    float inv_size = 1.0 / position_history.size();
    
    float index = position_history.size();
    int i = 0;
    for (deque<glm::vec3>::iterator it = position_history.begin(); it!=position_history.end(); ++it) {

      color_buffer_data[3 * i]     = draw_color.r * index * inv_size;
      color_buffer_data[3 * i + 1] = draw_color.g * index * inv_size;
      color_buffer_data[3 * i + 2] = draw_color.b * index * inv_size;

      vertex_buffer_data[3 * i]     = (*it).x;
      vertex_buffer_data[3 * i + 1] = (*it).y;
      vertex_buffer_data[3 * i + 2] = (*it).z;

      index--;
      i++;
    }

  }
  else if (flocker_draw_mode == DRAW_MODE_AXES) {

    num_vertices = 6;
    draw_mode = GL_LINES;

    vertex_buffer_data = (GLfloat *) malloc(3 * num_vertices * sizeof(GLfloat));
    color_buffer_data = (GLfloat *) malloc(3 * num_vertices * sizeof(GLfloat));

    double axis_scale = 0.25;

    // vertices

    vertex_buffer_data[0] = position.x;
    vertex_buffer_data[1] = position.y;
    vertex_buffer_data[2] = position.z;

    vertex_buffer_data[3] = position.x + axis_scale * frame_x.x;
    vertex_buffer_data[4] = position.y + axis_scale * frame_x.y;
    vertex_buffer_data[5] = position.z + axis_scale * frame_x.z;

    vertex_buffer_data[6] = position.x;
    vertex_buffer_data[7] = position.y;
    vertex_buffer_data[8] = position.z;

    vertex_buffer_data[9] = position.x + axis_scale * frame_y.x;
    vertex_buffer_data[10] = position.y + axis_scale * frame_y.y;
    vertex_buffer_data[11] = position.z + axis_scale * frame_y.z;

    vertex_buffer_data[12] = position.x;
    vertex_buffer_data[13] = position.y;
    vertex_buffer_data[14] = position.z;

    vertex_buffer_data[15] = position.x + axis_scale * frame_z.x;
    vertex_buffer_data[16] = position.y + axis_scale * frame_z.y;
    vertex_buffer_data[17] = position.z + axis_scale * frame_z.z;

    // color

    color_buffer_data[0] = 1.0f;
    color_buffer_data[1] = 1.0f;
    color_buffer_data[2] = 1.0f;

    color_buffer_data[3] = 1.0f;
    color_buffer_data[4] = 1.0f;
    color_buffer_data[5] = 1.0f;

    color_buffer_data[6] = 1.0f;
    color_buffer_data[7] = 1.0f;
    color_buffer_data[8] = 0.0f;

    color_buffer_data[9] = 1.0f;
    color_buffer_data[10] = 1.0f;
    color_buffer_data[11] = 0.0f;

    color_buffer_data[12] = 0.541f;
    color_buffer_data[13] = 0.169f;
    color_buffer_data[14] = 0.886f;

    color_buffer_data[15] = 0.541f;
    color_buffer_data[16] = 0.169f;
    color_buffer_data[17] = 0.886f;
    
  }
  else if (flocker_draw_mode == DRAW_MODE_POLY) {

    num_vertices = 12;
    draw_mode = GL_TRIANGLES;

    vertex_buffer_data = (GLfloat *) malloc(3 * num_vertices * sizeof(GLfloat));
    color_buffer_data = (GLfloat *) malloc(3 * num_vertices * sizeof(GLfloat));
    
    double width = 0.2f;
    double height = 0.2f;
    double length = 0.3f;
  
    // horizontal
    
    vertex_buffer_data[0] = position.x - 0.5f * width * frame_x.x;
    vertex_buffer_data[1] = position.y - 0.5f * width * frame_x.y;
    vertex_buffer_data[2] = position.z - 0.5f * width * frame_x.z;
    
    vertex_buffer_data[3] = position.x - 0.8f * length * frame_z.x;
    vertex_buffer_data[4] = position.y - 0.8f * length * frame_z.y;
    vertex_buffer_data[5] = position.z - 0.8f * length * frame_z.z;
        
    vertex_buffer_data[6] = position.x + 0.2f * length * frame_z.x;
    vertex_buffer_data[7] = position.y + 0.2f * length * frame_z.y;
    vertex_buffer_data[8] = position.z + 0.2f * length * frame_z.z;
    

    vertex_buffer_data[9] = position.x + 0.5f * width * frame_x.x;
    vertex_buffer_data[10] = position.y + 0.5f * width * frame_x.y;
    vertex_buffer_data[11] = position.z + 0.5f * width * frame_x.z;

    vertex_buffer_data[12] = position.x + 0.2f * length * frame_z.x;
    vertex_buffer_data[13] = position.y + 0.2f * length * frame_z.y;
    vertex_buffer_data[14] = position.z + 0.2f * length * frame_z.z;

    vertex_buffer_data[15] = position.x - 0.8f * length * frame_z.x;
    vertex_buffer_data[16] = position.y - 0.8f * length * frame_z.y;
    vertex_buffer_data[17] = position.z - 0.8f * length * frame_z.z;

    // vertical
    
    vertex_buffer_data[18] = position.x + 0.5f * height * frame_y.x;
    vertex_buffer_data[19] = position.y + 0.5f * height * frame_y.y;
    vertex_buffer_data[20] = position.z + 0.5f * height * frame_y.z;
    
    vertex_buffer_data[21] = position.x - 0.8f * length * frame_z.x;
    vertex_buffer_data[22] = position.y - 0.8f * length * frame_z.y;
    vertex_buffer_data[23] = position.z - 0.8f * length * frame_z.z;

    vertex_buffer_data[24] = position.x + 0.2f * length * frame_z.x;
    vertex_buffer_data[25] = position.y + 0.2f * length * frame_z.y;
    vertex_buffer_data[26] = position.z + 0.2f * length * frame_z.z;

    
    vertex_buffer_data[27] = position.x - 0.5f * height * frame_y.x;
    vertex_buffer_data[28] = position.y - 0.5f * height * frame_y.y;
    vertex_buffer_data[29] = position.z - 0.5f * height * frame_y.z;

    vertex_buffer_data[30] = position.x + 0.2f * length * frame_z.x;
    vertex_buffer_data[31] = position.y + 0.2f * length * frame_z.y;
    vertex_buffer_data[32] = position.z + 0.2f * length * frame_z.z;
   
    vertex_buffer_data[33] = position.x - 0.8f * length * frame_z.x;
    vertex_buffer_data[34] = position.y - 0.8f * length * frame_z.y;
    vertex_buffer_data[35] = position.z - 0.8f * length * frame_z.z;
    
    // color

    int i;

    for (i = 0; i < 6; i++) {
      color_buffer_data[3 * i]     = 1.0f;
      color_buffer_data[3 * i + 1] = 1.0f;
      color_buffer_data[3 * i + 2] = 1.0f;
    }

    for (i = 6; i < 9; i++) {
      color_buffer_data[3 * i]     = 1.0f;
      color_buffer_data[3 * i + 1] = 1.0f;
      color_buffer_data[3 * i + 2] = 0.0f;
    }

    for (i = 9; i < 12; i++) {
      color_buffer_data[3 * i]     = 0.541f;
      color_buffer_data[3 * i + 1] = 0.169f;
      color_buffer_data[3 * i + 2] = 0.886f;
    }
  }

  // Our ModelViewProjection : multiplication of our 3 matrices
  
  glm::mat4 MVP = ProjectionMat * ViewMat * Model;
  
  // make this transform available to shaders
  
  glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
  
  // 1st attribute buffer : vertices
  
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
  glBufferData(GL_ARRAY_BUFFER, 3 * num_vertices * sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW);
  
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
  glVertexAttribPointer(0,                  // attribute. 0 to match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);
  
  // 2nd attribute buffer : colors
  
  glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
  glBufferData(GL_ARRAY_BUFFER, 3 * num_vertices * sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);
  
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
  glVertexAttribPointer(1,                                // attribute. 1 to match the layout in the shader.
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
			);
  
  // Draw the flocker!
  
  glDrawArrays(draw_mode, 0, num_vertices);
  
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  
  free(vertex_buffer_data);
  free(color_buffer_data);
}

//----------------------------------------------------------------------------

void Predator::update()
{
    // set accelerations (aka forces)
    
    compute_hunger_force();
    acceleration = hunger_force;
    
    if (glm::length(acceleration) > 0)
        draw_color = glm::vec3(1.0f, 0.0f, 0.0f);
    else
        draw_color = glm::vec3(1.0f, 0.941f, 0.122f);

    // randomness

    if (random_force_limit > 0.0) {
        acceleration.x += uniform_random(-random_force_limit, random_force_limit);
        acceleration.y += uniform_random(-random_force_limit, random_force_limit);
        acceleration.z += uniform_random(-random_force_limit, random_force_limit);
    }

    // update velocity

    new_velocity = velocity + acceleration;   // scale acceleration by dt?

    // limit velocity

    double mag = glm::length(new_velocity);
    if (mag > MAX_FLOCKER_SPEED)
    new_velocity *= (float) (MAX_FLOCKER_SPEED / mag);

    // update position

    new_position = position + new_velocity;   // scale new_velocity by dt?
}

//----------------------------------------------------------------------------

bool Predator::compute_hunger_force() {
  int j;
  glm::vec3 direction;
  int count = 0;
  double mag, percent;
  double F;

  hunger_force = glm::vec3(0, 0, 0);

  double min;
  for (j = 0; j < p_to_f_squared_distance[index].size(); j++)
    if (p_to_f_squared_distance[index][j] >= min_squared_hunger_distance &&
	p_to_f_squared_distance[index][j] <= max_squared_hunger_distance) {

      // set (unweighted) force magnitude

      percent = (p_to_f_squared_distance[index][j] - max_squared_hunger_distance) * inv_range_squared_hunger_distance;
      F = 0.5 + -0.5 * cos(percent * 2.0 * M_PI);

      // set force direction

      direction = (float) F * glm::normalize(flocker_array[j]->position - position);   // opposite direction of hunger
      hunger_force += direction;
      count++;
    }

  if (count > 0) {
    hunger_force *= (float) hunger_weight;
    return true;
  }
  else
    return false;
}


//----------------------------------------------------------------------------

// attempt to be slightly efficient by pre-calculating all of the distances between
// predator and flockers exactly once

void calculate_p_to_f_squared_distances()
{
  int pred_i, flock_i;
  glm::vec3 diff;
  double len;

  for (pred_i = 0; pred_i < predator_array.size(); pred_i++)
    for (flock_i = 0; flock_i < flocker_array.size(); flock_i++) {
      diff = predator_array[pred_i]->position - flocker_array[flock_i]->position;
      len = glm::length2(diff);
      p_to_f_squared_distance[pred_i][flock_i] = len;
    }
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
