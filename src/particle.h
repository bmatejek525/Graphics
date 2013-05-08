// Include file for the particle system



// Particle system integration types

enum {
  EULER_INTEGRATION,
  MIDPOINT_INTEGRATION,
  ADAPTIVE_STEP_SIZE_INTEGRATION,
  RK4_INTEGRATION
};



// Particle system functions

void UpdateParticles(R3Scene *scene, double current_time, double delta_time, int integration_type);
void GenerateParticles(R3Scene *scene, double current_time, double delta_time);
void RenderParticles(R3Scene *scene, double current_time, double delta_time);

//void RenderTails(R3Scene *scene, double current_time, double delta_time);
//bool sphereIntersection(R3Sphere *sphere, R3Ray *ray, R3Intersection *intersection);
