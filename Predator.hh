#ifndef PREDATOR_HH

#define PREDATOR_HH

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

#include "Creature.hh"
#include "Flocker.hh"

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

#define MAX_PREDATOR_SPEED                           0.04

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

class Predator : public Creature
{
public:
  glm::vec3 hunger_force;
  
  double random_force_limit;
  
  double hunger_weight;
  double min_hunger_distance, min_squared_hunger_distance;
  double max_hunger_distance, max_squared_hunger_distance;
  double inv_range_squared_hunger_distance;
  
  Predator(int,                    // index
	   double, double, double, // initial position
	   double, double, double, // initial velocity
       double, double, double, // hunger specifications
	   float, float, float,    // base color
	   int = 1);               // number of past states to save

  void draw(glm::mat4);
  void update();
  bool compute_hunger_force();

};

//----------------------------------------------------------------------------

void calculate_p_to_f_squared_distances();

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

#endif
