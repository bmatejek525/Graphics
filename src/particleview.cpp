// Source file for the scene file viewer


////////////////////////////////////////////////////////////
// INCLUDE FILES
////////////////////////////////////////////////////////////
#include "Utilities.h"
#include "R3/R3.h"
#include "R3Scene.h"
#include "particle.h"
#include "cos426_opengl.h"
#include "player.h"
#include "bullet.h"
#include "boid.h"
#include "raytrace.h"
#include <sys/time.h>
#include <signal.h>

#if defined(__APPLE__)
#define LINUX 0
#else
#define LINUX 1
#endif

////////////////////////////////////////////////////////////
// GLOBAL CONSTANTS
////////////////////////////////////////////////////////////

static const double VIDEO_FRAME_DELAY = 1./25.; // 25 FPS

////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////////////////////////

void keyboard();
void GLUTDrawLargeRedText(const R3Point&, const char *);

double RandomNumber(void);

pid_t BSound = -1;

GLint UniformLocation;

bool useShader=false;
//vertex shader handle                                                          

pid_t child;                                                                                                   
static GLuint v;
//fragment shader handle                                                                                                                                                           
static GLuint f;
//shader program handle                                                                                                                                                            
//using zero value sets OpenGL back to using fixed pipeline                                                                                                                        
static GLuint shader=0;


// Program arguments

static char *input_scene_name = NULL;
static char *output_image_name = NULL;
static const char *video_prefix = "./video-frames/";
static int integration_type = EULER_INTEGRATION;

// track keypresses
bool* keyStates = new bool[256];



// Display variables

static R3Scene *scene = NULL;
static R3Camera camera;
static int show_faces = 1;
static int show_edges = 0;
static int show_bboxes = 0;
static int show_lights = 0;
static int show_camera = 0;
static int show_bullets = 1;
static int show_particles = 1;
static int show_players = 1;
static int show_particle_springs = 1;
static int show_particle_sources_and_sinks = 1;
//static int play_sounds = 0;
static int save_image = 0;
static int save_video = 0;
static int num_frames_to_record = -1;
static bool follow = false;
static bool view2 = true;
static bool view3 = false;
static bool view4 = false;
static int quit = 0;

static timeval last_boost_time;

// GLUT variables

static int GLUTwindow = 0;
static int GLUTwindow_height = 512 * 1.6;
static int GLUTwindow_width = 512 * 1.6;
static int GLUTmouse[2] = { 0, 0 };
static int GLUTbutton[3] = { 0, 0, 0 };
static int GLUTmodifiers = 0;

// GLUT command list

enum {
    DISPLAY_FACE_TOGGLE_COMMAND,
    DISPLAY_EDGE_TOGGLE_COMMAND,
    DISPLAY_BBOXES_TOGGLE_COMMAND,
    DISPLAY_LIGHTS_TOGGLE_COMMAND,
    DISPLAY_CAMERA_TOGGLE_COMMAND,
    DISPLAY_PARTICLES_TOGGLE_COMMAND,
    DISPLAY_PARTICLE_SPRINGS_TOGGLE_COMMAND,
    DISPLAY_PARTICLE_SOURCES_AND_SINKS_TOGGLE_COMMAND,
    SAVE_IMAGE_COMMAND,
    SAVE_VIDEO_COMMAND,
    QUIT_COMMAND,
};



////////////////////////////////////////////////////////////
// TIMER CODE
////////////////////////////////////////////////////////////

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/time.h>
#endif

static double GetTime(void)
{
#ifdef _WIN32
    // Return number of seconds since start of execution
    static int first = 1;
    static LARGE_INTEGER timefreq;
    static LARGE_INTEGER start_timevalue;
    
    // Check if this is the first time
    if (first) {
        // Initialize first time
        QueryPerformanceFrequency(&timefreq);
        QueryPerformanceCounter(&start_timevalue);
        first = 0;
        return 0;
    }
    else {
        // Return time since start
        LARGE_INTEGER current_timevalue;
        QueryPerformanceCounter(&current_timevalue);
        return ((double) current_timevalue.QuadPart -
                (double) start_timevalue.QuadPart) /
        (double) timefreq.QuadPart;
    }
#else
    // Return number of seconds since start of execution
    static int first = 1;
    static struct timeval start_timevalue;
    
    // Check if this is the first time
    if (first) {
        // Initialize first time
        gettimeofday(&start_timevalue, NULL);
        first = 0;
        return 0;
    }
    else {
        // Return time since start
        struct timeval current_timevalue;
        gettimeofday(&current_timevalue, NULL);
        int secs = current_timevalue.tv_sec - start_timevalue.tv_sec;
        int usecs = current_timevalue.tv_usec - start_timevalue.tv_usec;
        return (double) (secs + 1.0E-6F * usecs);
    }
#endif
}



////////////////////////////////////////////////////////////
// SCENE DRAWING CODE
////////////////////////////////////////////////////////////

void DrawShape(R3Shape *shape)
{
    // Check shape type
    if (shape->type == R3_BOX_SHAPE) shape->box->Draw();
    else if (shape->type == R3_SPHERE_SHAPE) shape->sphere->Draw();
    else if (shape->type == R3_CYLINDER_SHAPE) shape->cylinder->Draw();
    else if (shape->type == R3_CONE_SHAPE) shape->cone->Draw();
    else if (shape->type == R3_MESH_SHAPE) {
      shape->mesh->Draw();
    }
    else if (shape->type == R3_SEGMENT_SHAPE) shape->segment->Draw();
    else if (shape->type == R3_CIRCLE_SHAPE) shape->circle->Draw();
    else fprintf(stderr, "Unrecognized shape type: %d\n", shape->type);
}



void LoadMatrix(R3Matrix *matrix)
{
    // Multiply matrix by top of stack
    // Take transpose of matrix because OpenGL represents vectors with
    // column-vectors and R3 represents them with row-vectors
    R3Matrix m = matrix->Transpose();
    glMultMatrixd((double *) &m);
}



void LoadMaterial(R3Material *material)
{
    GLfloat c[4];
    
    // Check if same as current
    static R3Material *current_material = NULL;
    if (material == current_material) return;
    current_material = material;
    
    // Compute "opacity"
    double opacity = 1 - material->kt.Luminance();
    
    // Load ambient
    c[0] = material->ka[0];
    c[1] = material->ka[1];
    c[2] = material->ka[2];
    c[3] = opacity;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, c);
    
    // Load diffuse
    c[0] = material->kd[0];
    c[1] = material->kd[1];
    c[2] = material->kd[2];
    c[3] = opacity;
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, c);
    
    // Load specular
    c[0] = material->ks[0];
    c[1] = material->ks[1];
    c[2] = material->ks[2];
    c[3] = opacity;
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);
    
    // Load emission
    c[0] = material->emission.Red();
    c[1] = material->emission.Green();
    c[2] = material->emission.Blue();
    c[3] = opacity;
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, c);
    
    // Load shininess
    c[0] = material->shininess;
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, c[0]);
    
    // Load texture
    if (material->texture) {
        if (material->texture_index <= 0) {
            // Create texture in OpenGL
            GLuint texture_index;
            glGenTextures(1, &texture_index);
            material->texture_index = (int) texture_index;
            glBindTexture(GL_TEXTURE_2D, material->texture_index);
            R2Image *image = material->texture;
            int npixels = image->NPixels();
            R2Pixel *pixels = image->Pixels();
            GLfloat *buffer = new GLfloat [ 4 * npixels ];
            R2Pixel *pixelsp = pixels;
            GLfloat *bufferp = buffer;
            for (int j = 0; j < npixels; j++) {
                *(bufferp++) = pixelsp->Red();
                *(bufferp++) = pixelsp->Green();
                *(bufferp++) = pixelsp->Blue();
                *(bufferp++) = pixelsp->Alpha();
                pixelsp++;
            }
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glTexImage2D(GL_TEXTURE_2D, 0, 4, image->Width(), image->Height(), 0, GL_RGBA, GL_FLOAT, buffer);
            delete [] buffer;
        }
        
        // Select texture
        glBindTexture(GL_TEXTURE_2D, material->texture_index);
        glEnable(GL_TEXTURE_2D);
	glUniform1i(UniformLocation, 1);
    }
    else {
        glDisable(GL_TEXTURE_2D);
	glUniform1i(UniformLocation, 0);
    }
    
    // Enable blending for transparent surfaces
    if (opacity < 1) {
        glDepthMask(false);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
    }
    else {
        glDisable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ZERO);
        glDepthMask(true);
    }
}



void LoadCamera(R3Camera *camera)
{
    // Set projection transformation
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(2*180.0*camera->yfov/M_PI, (GLdouble) GLUTwindow_width /(GLdouble) GLUTwindow_height, 0.01, 10000);
    
    // Set camera transformation
    R3Vector t = -(camera->towards);
    R3Vector& u = camera->up;
    R3Vector& r = camera->right;
    GLdouble camera_matrix[16] = { r[0], u[0], t[0], 0, r[1], u[1], t[1], 0, r[2], u[2], t[2], 0, 0, 0, 0, 1 };
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMultMatrixd(camera_matrix);

    glTranslated(-(camera->eye[0]), -(camera->eye[1]), -(camera->eye[2]));
}



void LoadLights(R3Scene *scene)
{
    GLfloat buffer[4];
    
    // Load ambient light
    static GLfloat ambient[4];
    ambient[0] = scene->ambient[0];
    ambient[1] = scene->ambient[1];
    ambient[2] = scene->ambient[2];
    ambient[3] = 1;
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    
    // Load scene lights
    for (int i = 0; i < (int) scene->lights.size(); i++) {
        R3Light *light = scene->lights[i];
        int index = GL_LIGHT0 + i;
        
        // Temporarily disable light
        glDisable(index);
        
        // Load color
        buffer[0] = light->color[0];
        buffer[1] = light->color[1];
        buffer[2] = light->color[2];
        buffer[3] = 1.0;
        glLightfv(index, GL_DIFFUSE, buffer);
        glLightfv(index, GL_SPECULAR, buffer);
        
        // Load attenuation with distance
        buffer[0] = light->constant_attenuation;
        buffer[1] = light->linear_attenuation;
        buffer[2] = light->quadratic_attenuation;
        glLightf(index, GL_CONSTANT_ATTENUATION, buffer[0]);
        glLightf(index, GL_LINEAR_ATTENUATION, buffer[1]);
        glLightf(index, GL_QUADRATIC_ATTENUATION, buffer[2]);
        
        // Load spot light behavior
        buffer[0] = 180.0 * light->angle_cutoff / M_PI;
        buffer[1] = light->angle_attenuation;
        glLightf(index, GL_SPOT_CUTOFF, buffer[0]);
        glLightf(index, GL_SPOT_EXPONENT, buffer[1]);
        
        // Load positions/directions
        if (light->type == R3_DIRECTIONAL_LIGHT) {
            // Load direction
            buffer[0] = -(light->direction.X());
            buffer[1] = -(light->direction.Y());
            buffer[2] = -(light->direction.Z());
            buffer[3] = 0.0;
            glLightfv(index, GL_POSITION, buffer);
        }
        else if (light->type == R3_POINT_LIGHT) {
            // Load position
            buffer[0] = light->position.X();
            buffer[1] = light->position.Y();
            buffer[2] = light->position.Z();
            buffer[3] = 1.0;
            glLightfv(index, GL_POSITION, buffer);
        }
        else if (light->type == R3_SPOT_LIGHT) {
            // Load position
            buffer[0] = light->position.X();
            buffer[1] = light->position.Y();
            buffer[2] = light->position.Z();
            buffer[3] = 1.0;
            glLightfv(index, GL_POSITION, buffer);
            
            // Load direction
            buffer[0] = light->direction.X();
            buffer[1] = light->direction.Y();
            buffer[2] = light->direction.Z();
            buffer[3] = 1.0;
            glLightfv(index, GL_SPOT_DIRECTION, buffer);
        }
        else if (light->type == R3_AREA_LIGHT) {
            // Load position
            buffer[0] = light->position.X();
            buffer[1] = light->position.Y();
            buffer[2] = light->position.Z();
            buffer[3] = 1.0;
            glLightfv(index, GL_POSITION, buffer);
            
            // Load direction
            buffer[0] = light->direction.X();
            buffer[1] = light->direction.Y();
            buffer[2] = light->direction.Z();
            buffer[3] = 1.0;
            glLightfv(index, GL_SPOT_DIRECTION, buffer);
        }
        else {
            fprintf(stderr, "Unrecognized light type: %d\n", light->type);
            return;
        }
        
        // Enable light
        glEnable(index);
    }
}



void DrawNode(R3Scene *scene, R3Node *node)
{
    // Push transformation onto stack
    glPushMatrix();
    LoadMatrix(&node->transformation);
    
    // Load material
    if (node->material) LoadMaterial(node->material);
    
    if (node->shape) {
      if (node->shape->type == R3_SPHERE_SHAPE) {
	R3Vector cent = R3Point(0.0, 0.0, -120.0) - node->shape->sphere->Center();
	cent.Normalize();
	R3Vector motion = R3Vector(cent.Y(), -1.0 * cent.X(), 0.0) * .05;
	node->shape->sphere->Translate(motion);
      }
    }
    // Draw shape
    if (node->shape) DrawShape(node->shape);
    
    // Draw children nodes
    for (int i = 0; i < (int) node->children.size(); i++)
        DrawNode(scene, node->children[i]);
    
    // Restore previous transformation
    glPopMatrix();
    
    // Show bounding box
    if (show_bboxes) {
        GLboolean lighting = glIsEnabled(GL_LIGHTING);
        glDisable(GL_LIGHTING);
        node->bbox.Outline();
        if (lighting) glEnable(GL_LIGHTING);
    }
    
}



void DrawLights(R3Scene *scene)
{
    // Check if should draw lights
    if (!show_lights) return;
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glDisable(GL_LIGHTING);
    
    // Draw all lights
    double radius = scene->bbox.DiagonalRadius();
    for (int i = 0; i < scene->NLights(); i++) {
        R3Light *light = scene->Light(i);
        glColor3d(light->color[0], light->color[1], light->color[2]);
        if (light->type == R3_DIRECTIONAL_LIGHT) {
            // Draw direction vector
            glLineWidth(5);
            glBegin(GL_LINES);
            R3Point centroid = scene->bbox.Centroid();
            R3Vector vector = radius * light->direction;
            glVertex3d(centroid[0] - vector[0], centroid[1] - vector[1], centroid[2] - vector[2]);
            glVertex3d(centroid[0] - 1.25*vector[0], centroid[1] - 1.25*vector[1], centroid[2] - 1.25*vector[2]);
            glEnd();
            glLineWidth(1);
        }
        else if (light->type == R3_POINT_LIGHT) {
            // Draw sphere at point light position
            R3Sphere(light->position, 0.1 * radius).Draw();
        }
        else if (light->type == R3_SPOT_LIGHT) {
            // Draw sphere at point light position and line indicating direction
            R3Sphere(light->position, 0.1 * radius).Draw();
            
            // Draw direction vector
            glLineWidth(5);
            glBegin(GL_LINES);
            R3Vector vector = radius * light->direction;
            glVertex3d(light->position[0], light->position[1], light->position[2]);
            glVertex3d(light->position[0] + 0.25*vector[0], light->position[1] + 0.25*vector[1], light->position[2] + 0.25*vector[2]);
            glEnd();
            glLineWidth(1);
        }
        else if (light->type == R3_AREA_LIGHT) {
            // Draw circular area
            R3Vector v1, v2;
            double r = light->radius;
            R3Point p = light->position;
            int dim = light->direction.MinDimension();
            if (dim == 0) { v1 = light->direction % R3posx_vector; v1.Normalize(); v2 = light->direction % v1; }
            else if (dim == 1) { v1 = light->direction % R3posy_vector; v1.Normalize(); v2 = light->direction % v1; }
            else { v1 = light->direction % R3posz_vector; v1.Normalize(); v2 = light->direction % v1; }
            glBegin(GL_POLYGON);
            glVertex3d(p[0] +  1.00*r*v1[0] +  0.00*r*v2[0], p[1] +  1.00*r*v1[1] +  0.00*r*v2[1], p[2] +  1.00*r*v1[2] +  0.00*r*v2[2]);
            glVertex3d(p[0] +  0.71*r*v1[0] +  0.71*r*v2[0], p[1] +  0.71*r*v1[1] +  0.71*r*v2[1], p[2] +  0.71*r*v1[2] +  0.71*r*v2[2]);
            glVertex3d(p[0] +  0.00*r*v1[0] +  1.00*r*v2[0], p[1] +  0.00*r*v1[1] +  1.00*r*v2[1], p[2] +  0.00*r*v1[2] +  1.00*r*v2[2]);
            glVertex3d(p[0] + -0.71*r*v1[0] +  0.71*r*v2[0], p[1] + -0.71*r*v1[1] +  0.71*r*v2[1], p[2] + -0.71*r*v1[2] +  0.71*r*v2[2]);
            glVertex3d(p[0] + -1.00*r*v1[0] +  0.00*r*v2[0], p[1] + -1.00*r*v1[1] +  0.00*r*v2[1], p[2] + -1.00*r*v1[2] +  0.00*r*v2[2]);
            glVertex3d(p[0] + -0.71*r*v1[0] + -0.71*r*v2[0], p[1] + -0.71*r*v1[1] + -0.71*r*v2[1], p[2] + -0.71*r*v1[2] + -0.71*r*v2[2]);
            glVertex3d(p[0] +  0.00*r*v1[0] + -1.00*r*v2[0], p[1] +  0.00*r*v1[1] + -1.00*r*v2[1], p[2] +  0.00*r*v1[2] + -1.00*r*v2[2]);
            glVertex3d(p[0] +  0.71*r*v1[0] + -0.71*r*v2[0], p[1] +  0.71*r*v1[1] + -0.71*r*v2[1], p[2] +  0.71*r*v1[2] + -0.71*r*v2[2]);
            glEnd();
        }
        else {
            fprintf(stderr, "Unrecognized light type: %d\n", light->type);
            return;
        }
    }
    
    // Clean up
    if (lighting) glEnable(GL_LIGHTING);
}



void DrawCamera(R3Scene *scene)
{
    // Check if should draw lights
    if (!show_camera) return;
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glDisable(GL_LIGHTING);
    glColor3d(1.0, 1.0, 1.0);
    glLineWidth(5);
    
    // Draw view frustum
    R3Camera& c = scene->camera;
    double radius = scene->bbox.DiagonalRadius();
    R3Point org = c.eye + c.towards * radius;
    R3Vector dx = c.right * radius * tan(c.xfov);
    R3Vector dy = c.up * radius * tan(c.yfov);
    R3Point ur = org + dx + dy;
    R3Point lr = org + dx - dy;
    R3Point ul = org - dx + dy;
    R3Point ll = org - dx - dy;
    glBegin(GL_LINE_LOOP);
    glVertex3d(ur[0], ur[1], ur[2]);
    glVertex3d(ul[0], ul[1], ul[2]);
    glVertex3d(ll[0], ll[1], ll[2]);
    glVertex3d(lr[0], lr[1], lr[2]);
    glVertex3d(ur[0], ur[1], ur[2]);
    glVertex3d(c.eye[0], c.eye[1], c.eye[2]);
    glVertex3d(lr[0], lr[1], lr[2]);
    glVertex3d(ll[0], ll[1], ll[2]);
    glVertex3d(c.eye[0], c.eye[1], c.eye[2]);
    glVertex3d(ul[0], ul[1], ul[2]);
    glEnd();
    
    // Clean up
    glLineWidth(1);
    if (lighting) glEnable(GL_LIGHTING);
}



void DrawScene(R3Scene *scene)
{
    // Draw nodes recursively
    DrawNode(scene, scene->root);
}

void RenderBoids(R3Scene *scene, double current_time, double delta_time)
{

    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    // Define source material

    static R3Material source_material;
    if (source_material.id != 33) {
        source_material.ka.Reset(0.2,0.2,0.2,1);
        source_material.kd.Reset(1,1,1,1);
        source_material.ks.Reset(1,1,1,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(0,0,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
	//	source_material.texture = NULL;
	source_material.texture = new R2Image();
	if (!source_material.texture->Read("../input/boid.jpg")) {
	  fprintf(stderr, "trouble loading mesh\n");
	}
		//source_material.texture_index = -1;
	source_material.texture_index = 0;
        source_material.id = 33;
    }

    // Draw all particle sources
    glEnable(GL_LIGHTING);

    LoadMaterial(&source_material);
    for (int i = 0; i < (int)scene->boids.size(); i++) {
        R3Boid *boid = scene->boids[i];
        if (R3Distance(boid->pos,scene->players[0]->pos) > 25) boid->shape->mesh->Draw();
        else boid->shape2->mesh->Draw();
    }
    
    // Clean up
    if (!lighting) glDisable(GL_LIGHTING);

}
void RenderBullets(R3Scene *scene, double current_time, double delta_time)
{
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    // Define source material
    
    static R3Material source_material;
    if (source_material.id != 33) {
        source_material.ka.Reset(0.2,0.2,0.2,1);
        source_material.kd.Reset(0,1,0,1);
        source_material.ks.Reset(0,1,0,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(1,1,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
        source_material.texture = NULL;
        source_material.texture_index = -1;
        source_material.id = 33;
    }
    
    // Draw all particle sources
    glEnable(GL_LIGHTING);
    
    LoadMaterial(&source_material);
    for (int i = 0; i < (int)scene->bullets.size(); i++) {
        R3Bullet *bullet = scene->bullets[i];
        if (bullet->type == R3_MISSILE_BULLET) {
            bullet->shape->mesh->Draw();
        }
    }
    
    // Clean up
    if (!lighting) glDisable(GL_LIGHTING);
    
    
    
    // REGULAR BULLETS
    glDisable(GL_LIGHTING);
    glPointSize(5);
    glBegin(GL_POINTS);
    
    
    for (int i = 0; i < (int)scene->bullets.size(); i += 10) {
        R3Bullet *bullet = scene->bullets[i];
        if (bullet->type == R3_REGULAR_BULLET) {
            glColor3d(bullet->material->kd[0], bullet->material->kd[1], bullet->material->kd[2]);
            const R3Point& position = bullet->position;
            glVertex3d(position[0], position[1], position[2]);
        }
    }
    glEnd();
    
    
}


void DrawBoids(R3Scene *scene)
{

    // Get current time (in seconds) since start of execution
    double current_time = GetTime();
    static double previous_time = 0;
    
    
    static double time_lost_taking_videos = 0; // for switching back and forth
    // between recording and not
    // recording smoothly
    
    // program just started up?
    if (previous_time == 0) previous_time = current_time;
    
    // time passed since starting
    double delta_time = current_time - previous_time;
    
    // Update boids
    UpdateBoids(scene, delta_time);
    
    // Render Boids
    RenderBoids(scene, current_time - time_lost_taking_videos, delta_time);
    
    // Remember previous time
    previous_time = current_time;

}


void RenderPlayers(R3Scene *scene, double current_time, double delta_time)
{
    if (!show_players) return;
    // Draw every particle
    
    // REPLACE CODE HERE
    //glDisable(GL_LIGHTING);
    //glPointSize(5);
    //glBegin(GL_POINTS);
    
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    // Define source material
    static R3Material source_material;
    if (source_material.id != 33) {
        source_material.ka.Reset(0.2,0.2,0.2,1);
        source_material.kd.Reset(0,0,1,1);
        source_material.ks.Reset(0,1,0,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(0,0,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
	source_material.texture = NULL;
        source_material.texture_index = -1;
        source_material.id = 33;
    }
    
    // Draw all particle sources
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material);
    for (int i = 0; i < (int)scene->players.size(); i++) {
        R3Player *player = scene->players[i];
        player->shape->mesh->Draw();
    }
    
    
    
    // Clean up
    if (!lighting) glDisable(GL_LIGHTING);
}


void DrawPlayers(R3Scene *scene)
{
    // Get current time (in seconds) since start of execution
    double current_time = GetTime();
    static double previous_time = 0;
    
    
    static double time_lost_taking_videos = 0; // for switching back and forth
    // between recording and not
    // recording smoothly
    
    // program just started up?
    if (previous_time == 0) previous_time = current_time;
    
    // time passed since starting
    double delta_time = current_time - previous_time;
    
    // Update players
    UpdatePlayers(scene, current_time - time_lost_taking_videos, delta_time, integration_type);
    UpdateBullets(scene, current_time - time_lost_taking_videos, delta_time, integration_type);
    
    

    
    // Generate new particles
    //GenerateParticles(scene, current_time - time_lost_taking_videos, delta_time);

    /*    fprintf(stdout, "plane:");
    scene->players[0]->pos.Print();
    fprintf(stdout, "\ncam");
    camera.eye.Print();
    */
    if (follow || view2 || view3 || view4) {
      //      camera.eye = scene->players[0]->shape->mesh->Center();
	  if (scene->players.size() != 0) {
		  camera.eye = scene->players[0]->pos + 2.5 *scene->players[0]->nose;
		  if (view2 || view3 || view4) camera.eye = scene->players[0]->pos  -4.5 *scene->players[0]->nose ;
		  camera.towards = scene->players[0]->nose;
		  camera.right = scene->players[0]->wing;
		  camera.up = camera.right;
		  camera.up.Cross(camera.towards);
		  if (view2 || view3) camera.eye += .7*camera.up;
          if (view3) {
              camera.towards *= -1;
              camera.right *= -1;
              camera.up *= -1;
          }
          if (view4) {
              camera.eye -= .1 * camera.towards;
          }
          
		}
    }
    
    // Render players
    RenderPlayers(scene, current_time - time_lost_taking_videos, delta_time);

    glUseProgram(0);
    RenderBullets(scene, current_time - time_lost_taking_videos, delta_time);
    if (useShader)
      glUseProgram(shader);

    // Remember previous time
    previous_time = current_time;
}




void DrawParticles(R3Scene *scene)
{
    // Get current time (in seconds) since start of execution
    double current_time = GetTime();
    static double previous_time = 0;
    
    
    static double time_lost_taking_videos = 0; // for switching back and forth
    // between recording and not
    // recording smoothly
    
    // program just started up?
    if (previous_time == 0) previous_time = current_time;
    
    // time passed since starting
    double delta_time = current_time - previous_time;
    
    
    // Update particles
    UpdateParticles(scene, current_time - time_lost_taking_videos, delta_time, integration_type);
    
    // Generate new particles
    GenerateParticles(scene, current_time - time_lost_taking_videos, delta_time);
    
    glUseProgram(0);
    // Render particles
    if (show_particles) RenderParticles(scene, current_time - time_lost_taking_videos, delta_time);
    if (useShader)
      glUseProgram(shader);

    
    // Remember previous time
    previous_time = current_time;
}

void DrawBullets(R3Scene *scene)
{
    // Get current time (in seconds) since start of execution
    double current_time = GetTime();
    static double previous_time = 0;
    
    
    static double time_lost_taking_videos = 0; // for switching back and forth
    // between recording and not
    // recording smoothly
    
    // program just started up?
    if (previous_time == 0) previous_time = current_time;
    
    // time passed since starting
    double delta_time = current_time - previous_time;
    
    
    
    // Update particles
    UpdateBullets(scene, current_time - time_lost_taking_videos, delta_time, integration_type);
    
    // Generate new particles
    //GenerateParticles(scene, current_time - time_lost_taking_videos, delta_time);
    
    // Render particles
    if (show_bullets) RenderBullets(scene, current_time - time_lost_taking_videos, delta_time);
    
    // Remember previous time
    previous_time = current_time;
}


void DrawParticleSources(R3Scene *scene)
{
    // Check if should draw particle sources
    if (!show_particle_sources_and_sinks) return;
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    

	
    // Define source material
    static R3Material source_material;
    if (source_material.id != 33) {
        source_material.ka.Reset(0.2,0.2,0.2,1);
        source_material.kd.Reset(0,1,0,1);
        source_material.ks.Reset(0,1,0,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(0,0,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
        source_material.texture = NULL;
        source_material.texture_index = -1;
        source_material.id = 33;
    }
	
    // Draw all particle sources
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material);
    for (int i = 0; i < scene->NParticleSources(); i++) {
        R3ParticleSource *source = scene->ParticleSource(i);
        DrawShape(source->shape);
    }
    
    // Clean up
    if (!lighting) glDisable(GL_LIGHTING);
}

void killShotEnemy(R3Scene *scene, double delta_time) {
    if (scene->enemies.size() == 0)
        return;
    
    vector<int> deleteBullets;
    
    for (int i = 0; i < (int) scene->bullets.size(); i++) {
        
        R3Ray *ray = new R3Ray(scene->bullets[i]->position, scene->bullets[i]->velocity);
        double intersection = meshIntersection(scene->enemies[0]->shape->mesh, ray);
        if (intersection < scene->bullets[i]->velocity.Length() * delta_time) {
            if (scene->bullets[i]->type == R3_REGULAR_BULLET) {
                scene->enemies[0]->health -= 0.05;
                if (scene->bullets.size() % 10 == 0) {
                    if (scene->boids.size() < 25) {
                        GenerateBoids(scene,1,20);
                    }
                }
            }
            else {
                scene->enemies[0]->health -= 15.0;
                if (scene->boids.size() < 25) {
                    GenerateBoids(scene,1,20);
                }
            }
            
            scene->bullets.erase(scene->bullets.begin() + i);
            i--;
            if (scene->enemies[0]->health < 0) {
                // PRINT WIN MESSAGE
                Explode(scene, scene->enemies[0]);
                return;
            }
        }
    }
}


void DrawEnemies(R3Scene *scene)
{
  if (scene->enemies.size() == 0)
    return;

  // Get current time (in seconds) since start of execution
  double current_time = GetTime();
  static double previous_time = 0;
	
  // Setup
  GLboolean lighting = glIsEnabled(GL_LIGHTING);
  glEnable(GL_LIGHTING);

  // Define source material
  static R3Material enemy_material;
  if (enemy_material.id != 33) {
    enemy_material.ka.Reset(0.2,0.2,0.2,1);
    enemy_material.kd.Reset(0.45,0.45,0.45,1);
    enemy_material.ks.Reset(0.45,0.45,0.45,1);
    enemy_material.kt.Reset(0,0,0,1);
    enemy_material.emission.Reset(0,0,0,1);
    enemy_material.shininess = 1;
    enemy_material.indexofrefraction = 1;
    //    enemy_material.texture = NULL;
    enemy_material.texture = new R2Image();
    if (!enemy_material.texture->Read("../input/paper.jpg")) {
      fprintf(stderr, "oops\n");
    }
    enemy_material.texture_index = -1;
    enemy_material.id = 33;
  }
  // program just started up?
  if (previous_time == 0) previous_time = current_time;
    
  // time passed since starting
  double delta_time = current_time - previous_time;
  
  killShotEnemy(scene, delta_time);
  if (scene->enemies.size() == 0)
    return;
  
  // Draw all particle sources
  glEnable(GL_LIGHTING);
  LoadMaterial(&enemy_material);
  for (unsigned int i = 0; i < scene->enemies.size(); i++) {
    R3Enemy *enemy = scene->enemies[i];
    double speed = 0.001 + 0.002 * ((100 - enemy->health) / 100);
    // update the center position
    enemy->shape->mesh->Rotate(speed, R3Line(enemy->shape->mesh->Center(), enemy->direction));
    enemy->shape->mesh->Rotate(speed, R3Line(R3Point(0, 0, 0), R3Vector(0, 0, -1)));
    enemy->direction.Rotate(R3Vector(0, 0, -1), speed);
    DrawShape(enemy->shape);
  }
  // Clean up
  if (!lighting) glDisable(GL_LIGHTING);
  
  previous_time = current_time;
}



void DrawParticleSinks(R3Scene *scene)
{
    // Check if should draw particle sinks
    if (!show_particle_sources_and_sinks) return;
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    // Define sink material
    static R3Material sink_material;
    if (sink_material.id != 33) {
        sink_material.ka.Reset(0.2,0.2,0.2,1);
        sink_material.kd.Reset(1,0,0,1);
        sink_material.ks.Reset(1,0,0,1);
        sink_material.kt.Reset(0,0,0,1);
        sink_material.emission.Reset(0,0,0,1);
        sink_material.shininess = 1;
        sink_material.indexofrefraction = 1;
        sink_material.texture = NULL;
        sink_material.texture_index = -1;
        sink_material.id = 33;
    }
    
    // Draw all particle sinks
    glEnable(GL_LIGHTING);
    LoadMaterial(&sink_material);
    for (int i = 0; i < scene->NParticleSinks(); i++) {
        R3ParticleSink *sink = scene->ParticleSink(i);
        DrawShape(sink->shape);
    }
    
    // Clean up
    if (!lighting) glDisable(GL_LIGHTING);
}



void DrawParticleSprings(R3Scene *scene)
{
    // Check if should draw particle springs
    if (!show_particle_springs) return;
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glDisable(GL_LIGHTING);
    
    // Draw all particle sources
    glColor3d(0.5, 0.5, 0.5);
    glBegin(GL_LINES);
    for (unsigned int i = 0; i < scene->particle_springs.size(); i++) {
        R3ParticleSpring *spring = scene->particle_springs[i];
        const R3Point& p0 = spring->particles[0]->position;
        const R3Point& p1 = spring->particles[1]->position;
        glVertex3d(p0[0], p0[1], p0[2]);
        glVertex3d(p1[0], p1[1], p1[2]);
    }
    glEnd();
    
    // Clean up
    if (lighting) glEnable(GL_LIGHTING);
}




////////////////////////////////////////////////////////////
// GLUT USER INTERFACE CODE
////////////////////////////////////////////////////////////

void GLUTMainLoop(void)
{
    // Run main loop -- never returns
    glutMainLoop();
}



void GLUTDrawText(const R3Point& p, const char *s)
{
    // Draw text string s and position p

//    glColor3f(0.0, 1.0, 0.0);
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    
    static R3Material source_material;
    // Define source material
    if (source_material.id != 33) {
        source_material.ka.Reset(0.2,0.2,0.2,1);
        source_material.kd.Reset(0,1,0,1);
        source_material.ks.Reset(0,1,0,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(0,1,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
        source_material.texture = NULL;
        source_material.texture_index = -1;
        source_material.id = 33;
    }
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material);
    
    
    glRasterPos3d(p[0], p[1], p[2]);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *(s++));
    
    // Clean up
    glLineWidth(1);
    if (lighting) glEnable(GL_LIGHTING);

}

void GLUTDrawRedText(const R3Point& p, const char *s)
{
    // Draw text string s and position p
    
    //    glColor3f(0.0, 1.0, 0.0);
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    
    static R3Material source_material;
    // Define source material
    if (source_material.id != 33) {
        source_material.ka.Reset(0.2,0.2,0.2,1);
        source_material.kd.Reset(0,0,0,1);
        source_material.ks.Reset(0,0,0,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(1,0,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
        source_material.texture = NULL;
        source_material.texture_index = -1;
        source_material.id = 33;
    }
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material);
    
    
    glRasterPos3d(p[0], p[1], p[2]);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *(s++));
    
    // Clean up
    glLineWidth(1);
    if (lighting) glEnable(GL_LIGHTING);
    
}
 
void GLUTDrawLargeText(const R3Point& p, const char *s)
{
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    
    static R3Material source_material;
    // Define source material
    if (source_material.id != 33) {
        source_material.ka.Reset(0.2,0.2,0.2,1);
        source_material.kd.Reset(0,1,0,1);
        source_material.ks.Reset(0,1,0,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(0,1,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
        source_material.texture = NULL;
        source_material.texture_index = -1;
        source_material.id = 33;
    }
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material);
    
    glRasterPos3d(p[0], p[1], p[2]);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *(s++));
    
    // Clean up
    glLineWidth(1);
    if (lighting) glEnable(GL_LIGHTING);
    
}



void GLUTSaveImage(const char *filename)
{
    // Create image
    R2Image image(GLUTwindow_width, GLUTwindow_height);
    
    // Read screen into buffer
    GLfloat *pixels = new GLfloat [ 3 * GLUTwindow_width * GLUTwindow_height ];
    glReadPixels(0, 0, GLUTwindow_width, GLUTwindow_height, GL_RGB, GL_FLOAT, pixels);
    
    // Load pixels from frame buffer
    GLfloat *pixelsp = pixels;
    for (int j = 0; j < GLUTwindow_height; j++) {
        for (int i = 0; i < GLUTwindow_width; i++) {
            double r = (double) *(pixelsp++);
            double g = (double) *(pixelsp++);
            double b = (double) *(pixelsp++);
            R2Pixel pixel(r, g, b, 1);
            image.SetPixel(i, j, pixel);
        }
    }
    
    // Write image to file
    image.Write(filename);
    
    // Delete buffer
    delete [] pixels;
}



void GLUTStop(void)
{
    // Destroy window
    glutDestroyWindow(GLUTwindow);
    
    // Delete scene
    delete scene;
    
    // Exit
    exit(0);
}



void GLUTIdle(void)
{
    // Set current window
    if ( glutGetWindow() != GLUTwindow )
        glutSetWindow(GLUTwindow);


    
    // Redraw
    glutPostRedisplay();
}



void GLUTResize(int w, int h)
{
    // Resize window
    glViewport(0, 0, w, h);
    
    // Resize camera vertical field of view to match aspect ratio of viewport
    camera.yfov = atan(tan(camera.xfov) * (double) h/ (double) w);
    
    // Remember window size
    GLUTwindow_width = w;
    GLUTwindow_height = h;
    
    // Redraw
    glutPostRedisplay();
}

void DisplayYouLose(R3Scene *scene) {
    R3Point p1 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p2 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) + (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p3 = (camera.eye + (camera.neardist * camera.towards) + (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    
    double depth = 2e-12;
    
    double y = GLUTwindow_height * .5;
    double x = GLUTwindow_width * .45;
    //create ray through each pixel
    R3Vector upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    R3Vector acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    R3Point p = p1 + upVector + acrossVector;
    R3Vector vector = p - camera.eye;
    vector.Normalize();
    R3Point p4 = p + depth * vector;
    
    const char* buffer = "You Lose";
    GLUTDrawLargeRedText(p4, buffer);
    
    
}


void DisplayYouWin(R3Scene *scene) {
    R3Point p1 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p2 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) + (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p3 = (camera.eye + (camera.neardist * camera.towards) + (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    
    double depth = 2e-12;
    
    double y = GLUTwindow_height * .5;
    double x = GLUTwindow_width * .45;
    //create ray through each pixel
    R3Vector upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    R3Vector acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    R3Point p = p1 + upVector + acrossVector;
    R3Vector vector = p - camera.eye;
    vector.Normalize();
    R3Point p4 = p + depth * vector;
    
    const char* buffer = "You Win!";
    GLUTDrawLargeText(p4, buffer);
    
    
}


void DisplayBoundaryWarning(R3Scene *scene) {
    R3Point p1 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p2 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) + (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p3 = (camera.eye + (camera.neardist * camera.towards) + (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    
    double depth = 2e-12;
    
    double y = GLUTwindow_height * .5;
    double x = GLUTwindow_width * .30;
    //create ray through each pixel
    R3Vector upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    R3Vector acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    R3Point p = p1 + upVector + acrossVector;
    R3Vector vector = p - camera.eye;
    vector.Normalize();
    R3Point p4 = p + depth * vector;
    
    const char* buffer = "Leaving Battlefield! Turn Around!";
    GLUTDrawLargeText(p4, buffer);
    
    
}

void DisplayVelocity(R3Scene *scene) {
    
    double depth = 2e-12;
    
    R3Point p1 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p2 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) + (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p3 = (camera.eye + (camera.neardist * camera.towards) + (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    
    double y = GLUTwindow_height * .95;
    double x = GLUTwindow_width * .85;
    //create ray through each pixel
    R3Vector upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    R3Vector acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    R3Point p = p1 + upVector + acrossVector;
    R3Vector vector = p - camera.eye;
    vector.Normalize();
    R3Point p4 = p + depth * vector;
    
    char buffer [50];
    sprintf (buffer, "Velocity = %4.2f", scene->players[0]->velocity);
    GLUTDrawText(p4, buffer);
    
}

void DisplayBoidsKilled(R3Scene *scene) {
    
    double depth = 2e-12;
    
    R3Point p1 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p2 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) + (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p3 = (camera.eye + (camera.neardist * camera.towards) + (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    
    double y = GLUTwindow_height * .91;
    double x = GLUTwindow_width * .85;
    //create ray through each pixel
    R3Vector upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    R3Vector acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    R3Point p = p1 + upVector + acrossVector;
    R3Vector vector = p - camera.eye;
    vector.Normalize();
    R3Point p4 = p + depth* vector;
    
    char buffer [50];
    sprintf (buffer, "Boids Killed = %d", scene->players[0]->boidsKilled);
    GLUTDrawText(p4, buffer);
    
}

void DisplayMissileCount(R3Scene *scene) {
    
    double depth = 2e-12;
    
    R3Point p1 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p2 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) + (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p3 = (camera.eye + (camera.neardist * camera.towards) + (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    
    double y = GLUTwindow_height * .87;
    double x = GLUTwindow_width * .85;
    //create ray through each pixel
    R3Vector upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    R3Vector acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    R3Point p = p1 + upVector + acrossVector;
    R3Vector vector = p - camera.eye;
    vector.Normalize();
    R3Point p4 = p + depth* vector;
    
    char buffer [50];
    sprintf (buffer, "Missiles: %d", scene->players[0]->missiles);
    GLUTDrawText(p4, buffer);
    
}

void DrawCrossHairs(R3Scene *scene) {
    
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    
    double intersection = 200;
    bool updatedInter = false; 
    for (unsigned int i = 0; i < scene->boids.size(); i++) {
        R3Ray *ray = new R3Ray(scene->players[0]->pos, scene->players[0]->nose);
        double current = boxIntersection(scene->boids[i]->shape->mesh->bbox, ray);
        if ((current < intersection) && (current != -1)) {
            intersection = current;
            updatedInter = true; 
        }
    }
    for (unsigned int i = 0; i < scene->enemies.size(); i++) {
        R3Ray *ray = new R3Ray(scene->players[0]->pos, scene->players[0]->nose);
        
        double current = boxIntersection(scene->enemies[i]->shape->mesh->bbox, ray);
        if (current < intersection) {
            intersection = current;
            updatedInter = true;
        }
    }
    
    if (!updatedInter)
        intersection = 50; 

    double factor = .0057 * intersection + .029;
    
    R3Point startPos = scene->players[0]->pos + .9*intersection*scene->players[0]->nose;
    R3Point verticalTop = startPos + factor*camera.up;
    R3Point verticalBottom = startPos - factor*camera.up;
    R3Point horizRight = startPos + factor*camera.right;
    R3Point horizLeft = startPos - factor*camera.right;
    
    

    static R3Material source_material;
    // Define source material
    if (source_material.id != 33) {
        source_material.ka.Reset(0,0,0,1);
        source_material.kd.Reset(0,0,0,1);
        source_material.ks.Reset(0,0,0,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(0,1,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
        source_material.texture = NULL;
        source_material.texture_index = -1;
        source_material.id = 33;
    }
    if (updatedInter)
        source_material.emission.Reset(1,0,0,1);
    else
        source_material.emission.Reset(0,1,0,1);
    
    
    if (updatedInter) {
        char buffer [50];
        sprintf (buffer, "%4.2f", intersection);
        GLUTDrawRedText(horizRight, buffer);
    }
    
    if (updatedInter)
        source_material.emission.Reset(1,0,0,1);
    else
        source_material.emission.Reset(0,1,0,1);
    
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material);

    
    glLineWidth(2.5);
    glBegin(GL_LINES);
    glVertex3f(verticalTop.X(), verticalTop.Y(), verticalTop.Z());
    glVertex3f(verticalBottom.X(), verticalBottom.Y(), verticalBottom.Z());
    
    glVertex3f(horizLeft.X(), horizLeft.Y(), horizLeft.Z());
    glVertex3f(horizRight.X(), horizRight.Y(), horizRight.Z()); 
    glEnd();

    
    // Clean up
    glLineWidth(1);
    if (lighting) glEnable(GL_LIGHTING);
    
}

void GLUTDrawLargeRedText(const R3Point& p, const char *s)
{
    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    
    static R3Material source_material;
    // Define source material
    if (source_material.id != 33) {
        source_material.ka.Reset(0.2,0.2,0.2,1);
        source_material.kd.Reset(0,0,0,1);
        source_material.ks.Reset(0,0,0,1);
        source_material.kt.Reset(0,0,0,1);
        source_material.emission.Reset(1,0,0,1);
        source_material.shininess = 1;
        source_material.indexofrefraction = 1;
        source_material.texture = NULL;
        source_material.texture_index = -1;
        source_material.id = 33;
    }
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material);
    
    glRasterPos3d(p[0], p[1], p[2]);
    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *(s++));
    
    // Clean up
    glLineWidth(1);
    if (lighting) glEnable(GL_LIGHTING);
    
}




void DrawBoostAndHealthBar(R3Scene *scene) {

    // Setup
    GLboolean lighting = glIsEnabled(GL_LIGHTING);
    glEnable(GL_LIGHTING);
    
    //these points are used for creating rays through each pixel
    R3Point p1 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p2 = (camera.eye + (camera.neardist * camera.towards) - (camera.neardist * tan(camera.xfov) * camera.right) + (camera.neardist * tan(camera.yfov) * camera.up));
    R3Point p3 = (camera.eye + (camera.neardist * camera.towards) + (camera.neardist * tan(camera.xfov) * camera.right) - (camera.neardist * tan(camera.yfov) * camera.up));
    
    double depth = 2e-12;
    double height = .02;
    double bottom = .025;
    double left = .05;
    double right = .95;
    
    double y = bottom* GLUTwindow_height;
    double x = left *GLUTwindow_width;
    
    //corner one
    R3Vector upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    R3Vector acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    R3Point p = p1 + upVector + acrossVector;
    R3Vector vector = p - camera.eye;
    vector.Normalize();
    
    
    R3Point bl = p + depth * vector;
    
    //corner 2
    x = max(left, scene->players[0]->boost/100. *right) * GLUTwindow_width;
    acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point br = p + depth * vector;
    
    //corner 3
    y = (height + bottom) * GLUTwindow_width;
    upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point tr = p + depth * vector;
    
    
    //corner 4
    x = left * GLUTwindow_width;
    acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point tl = p + depth * vector;
    
    

    if (scene->players[0]->accel) {
        static R3Material source_material;
        // Define source material
        if (source_material.id != 33) {
            source_material.ka.Reset(0,0,0,1);
            source_material.kd.Reset(0,0,0,1);
            source_material.ks.Reset(0,0,0,1);
            source_material.kt.Reset(0,0,0,1);
            source_material.emission.Reset(0,1,0,1);
            source_material.shininess = 1;
            source_material.indexofrefraction = 1;
            source_material.texture = NULL;
            source_material.texture_index = -1;
            source_material.id = 33;
        }
        glEnable(GL_LIGHTING);
        LoadMaterial(&source_material);
    }
    else {
        static R3Material source_material2;
        // Define source material
        if (source_material2.id != 33) {
            source_material2.ka.Reset(0,0,0,1);
            source_material2.kd.Reset(0,0,0,1);
            source_material2.ks.Reset(0,0,0,1);
            source_material2.kt.Reset(0,0,0,1);
            source_material2.emission.Reset(1,0,0,1);
            source_material2.shininess = 1;
            source_material2.indexofrefraction = 1;
            source_material2.texture = NULL;
            source_material2.texture_index = -1;
            source_material2.id = 33;
        }
        glEnable(GL_LIGHTING);
        LoadMaterial(&source_material2);
    }
    
    // Draw all particle sources

    
    glBegin(GL_QUADS);                      // Draw A Quad
    glVertex3f(tl.X(), tl.Y(), tl.Z());           // Top Left
    glVertex3f(tr.X(), tr.Y(), tr.Z());           // Top Right
    glVertex3f(br.X(), br.Y(), br.Z());           // Bottom Right
    glVertex3f(bl.X(), bl.Y(), bl.Z());           // Bottom Left
    glEnd();
    
    
//    if ((left <= scene->players[0]->boost/100. *right) && (scene->players[0]->accel)) {
//        scene->players[0]->boost = 0;
//        scene->players[0]->accel = false;
//    }
    
    /*
    char buffer [50];
    sprintf (buffer, "Boost = %4.0f %%", scene->players[0]->boost);
//    bl.SetY(bl.Y() + .03);
    GLUTDrawText(bl, buffer);
    */
    
    bottom = bottom + 1.4 * height;
    //draw health bar
    y = bottom* GLUTwindow_height;
    x = left *GLUTwindow_width;
    
    //corner one
    upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    
    R3Point hbl = p + depth * vector;
    
    //corner 2
    x = max(left, scene->players[0]->health/100. *right) * GLUTwindow_width;
    acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point hbr = p + depth * vector;
    
    //corner 3
    y = (bottom + height) * GLUTwindow_width;
    upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point htr = p + depth * vector;
    
    
    //corner 4
    x = left * GLUTwindow_width;
    acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point htl = p + depth * vector;
    
    
    
    // Define source material
    static R3Material source_material1;
    if (source_material1.id != 33) {
        source_material1.ka.Reset(0.2,0.2,0.2,1);
        source_material1.kd.Reset(0,0,0,1);
        source_material1.ks.Reset(0,0,0,1);
        source_material1.kt.Reset(0,0,0,1);
        source_material1.emission.Reset(0,0,1,1);
        source_material1.shininess = 1;
        source_material1.indexofrefraction = 1;
        source_material1.texture = NULL;
        source_material1.texture_index = -1;
        source_material1.id = 33;
    }
    
    // Draw all particle sources
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material1);
    
    glBegin(GL_QUADS);                               // Draw A Quad
    glVertex3f(htl.X(), htl.Y(), htl.Z());           // Top Left
    glVertex3f(htr.X(), htr.Y(), htr.Z());           // Top Right
    glVertex3f(hbr.X(), hbr.Y(), hbr.Z());           // Bottom Right
    glVertex3f(hbl.X(), hbl.Y(), hbl.Z());           // Bottom Left
    glEnd();
    
    /*
    sprintf (buffer, "Health = %4.0f %%", scene->players[0]->health);
 //   hbl.SetY(hbl.Y() + .03);
    GLUTDrawText(hbl, buffer);
     */

    double top = 0.98;

    /* Draw Health Bar for Main Boss */
    y = top * GLUTwindow_height;
    x = left * GLUTwindow_width;
    
    //corner one
    upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    
    R3Point ebl = p + depth * vector;
    
    //corner 2
    x = max(left, scene->enemies[0]->health/100. *right) * GLUTwindow_width;
    acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point ebr = p + depth * vector;
    
    //corner 3
    y = (top + height) * GLUTwindow_width;
    upVector = (p2 - p1) * ((y + .5)/GLUTwindow_height);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point etr = p + depth * vector;
    
    
    //corner 4
    x = left * GLUTwindow_width;
    acrossVector = (p3 - p1) * ((x + .5)/GLUTwindow_width);
    
    p = p1 + upVector + acrossVector;
    vector = p - camera.eye;
    vector.Normalize();
    
    R3Point etl = p + depth * vector;
        
    // Define source material
    static R3Material source_material_enemy;
    if (source_material_enemy.id != 33) {
        source_material_enemy.ka.Reset(0.2,0.2,0.2,1);
        source_material_enemy.kd.Reset(1,0,0,1);
        source_material_enemy.ks.Reset(1,0,0,1);
        source_material_enemy.kt.Reset(0,0,0,1);
        source_material_enemy.emission.Reset(1,0,0,1);
        source_material_enemy.shininess = 1;
        source_material_enemy.indexofrefraction = 1;
        source_material_enemy.texture = NULL;
        source_material_enemy.texture_index = -1;
        source_material_enemy.id = 33;
    }
    
    // Draw all particle sources
    glEnable(GL_LIGHTING);
    LoadMaterial(&source_material_enemy);
    
    glBegin(GL_QUADS);                               // Draw A Quad
    glVertex3f(etl.X(), etl.Y(), etl.Z());           // Top Left
    glVertex3f(etr.X(), etr.Y(), etr.Z());           // Top Right
    glVertex3f(ebr.X(), ebr.Y(), ebr.Z());           // Bottom Right
    glVertex3f(ebl.X(), ebl.Y(), ebl.Z());           // Bottom Left
    glEnd();
    

    // Clean up
    glLineWidth(1);
    if (lighting) glEnable(GL_LIGHTING);
}

void setUpNewLevel(R3Scene *);



void GLUTRedraw(void)
{
    keyboard();
    
    // Initialize OpenGL drawing modes
    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDepthMask(true);
    
    // Clear window
    R3Rgb background = scene->background;
    glClearColor(background[0], background[1], background[2], background[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Load camera
    LoadCamera(&camera);
    
    // Load scene lights
    LoadLights(scene);
    

    
    if (scene->players[0]->health <= 0) {
        DisplayYouLose(scene);
        view2 = 0;
        follow = 0;
        view3 = 0;
        view4 = 1;
    }
    else if (scene->enemies[0]->health <= 0) {
        DisplayYouWin(scene);
        view2 = 0;
        follow = 0;
        view3 = 0;
        view4 = 0; 
    }
    else if (R3Distance(scene->center, scene->players[0]->pos) > .9 * scene->radius)
        DisplayBoundaryWarning(scene);
    else if (view2 || follow)
        DrawCrossHairs(scene);
    
        
        //Display velocity
    DisplayVelocity(scene);
    
    //Display number of boids killed
    DisplayBoidsKilled(scene);
    
    //Display number of available missiles
    DisplayMissileCount(scene);
    
    //draw boost bar
    DrawBoostAndHealthBar(scene);
    


    
    // Draw scene camera
    DrawCamera(scene);
    
    // Draw scene lights
    DrawLights(scene);
    


    
    // Draw particles
    DrawParticles(scene);
    
    if (scene->players[0]->health > 0)
        DrawPlayers(scene);
    DrawBoids(scene);


   
    // Draw particle sources
    DrawParticleSources(scene);
    
    // Draw particle sinks
    DrawParticleSinks(scene);
    
    // Draw particle springs
    DrawParticleSprings(scene);
    



	// Draw enemies
	DrawEnemies(scene);

    // Draw scene surfaces
    if (show_faces) {
        glEnable(GL_LIGHTING);
        DrawScene(scene);
    }
    
    // Draw scene edges
    if (show_edges) {
        glDisable(GL_LIGHTING);
        glColor3d(1 - background[0], 1 - background[1], 1 - background[2]);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        DrawScene(scene);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    // Save image
    if (save_image) {
        char image_name[256];
        static int image_number = 1;
        for (;;) {
            sprintf(image_name, "image%d.jpg", image_number++);
            FILE *fp = fopen(image_name, "r");
            if (!fp) break;
            else fclose(fp);
        }
        GLUTSaveImage(image_name);
        printf("Saved %s\n", image_name);
        save_image = 0;
    }
    
    // Save video
    if (save_video) {
        char frame_name[512];
        static int next_frame = 0;
        static int num_frames_recorded = 0;
        for (;;) {
            sprintf(frame_name, "%sframe%04d.jpg", video_prefix, next_frame++);
            FILE *fp = fopen(frame_name, "r");
            if (!fp) break;
            else fclose(fp);
        }
        GLUTSaveImage(frame_name);
        if (next_frame % 100 == 1) {
            printf("Saved %s\n", frame_name);
        }
        if (num_frames_to_record == ++num_frames_recorded) {
            save_video = 0;
            printf("Recorded %d frames, stopping as instructed.\n", num_frames_recorded);
            quit = 1;
        }
    }


  

    
    // Quit here so that can save image before exit
    if (quit) {
        if (output_image_name) GLUTSaveImage(output_image_name);
        GLUTStop();
    }
    
    // Swap buffers
    glutSwapBuffers();
}



void GLUTMotion(int x, int y)
{
    
    // Invert y coordinate
    y = GLUTwindow_height - y;
    
    // Compute mouse movement
    int dx = x - GLUTmouse[0];
    int dy = y - GLUTmouse[1];
    
    // Process mouse motion event
    if ((dx != 0) || (dy != 0)) {
        R3Point scene_center = scene->bbox.Centroid();
        if ((GLUTbutton[0] && (GLUTmodifiers & GLUT_ACTIVE_SHIFT)) || GLUTbutton[1]) {
            // Scale world?
            double factor = (double) dx / (double) GLUTwindow_width;
            factor += (double) dy / (double) GLUTwindow_height;
            factor = exp(2.0 * factor);
            factor = (factor - 1.0) / factor;
            R3Vector translation = (scene_center - camera.eye) * factor;
            camera.eye += translation;
            glutPostRedisplay();
        }
        else if (GLUTbutton[0] && (GLUTmodifiers & GLUT_ACTIVE_CTRL)) {
            // Translate world
            double length = R3Distance(scene_center, camera.eye) * tan(camera.yfov);
            double vx = length * (double) dx / (double) GLUTwindow_width;
            double vy = length * (double) dy / (double) GLUTwindow_height;
            R3Vector translation = -((camera.right * vx) + (camera.up * vy));
            camera.eye += translation;
            glutPostRedisplay();
        }
        else if (GLUTbutton[0]) {
            // Rotate world
            double vx = (double) dx / (double) GLUTwindow_width;
            double vy = (double) dy / (double) GLUTwindow_height;
            double theta = 4.0 * (fabs(vx) + fabs(vy));
            R3Vector vector = (camera.right * vx) + (camera.up * vy);
            R3Vector rotation_axis = camera.towards % vector;
            rotation_axis.Normalize();
            camera.eye.Rotate(R3Line(scene_center, rotation_axis), theta);
            camera.towards.Rotate(rotation_axis, theta);
            camera.up.Rotate(rotation_axis, theta);
            camera.right = camera.towards % camera.up;
            camera.up = camera.right % camera.towards;
            camera.towards.Normalize();
            camera.up.Normalize();
            camera.right.Normalize();
            glutPostRedisplay();
        }
    }

    
    // Remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;
    
}



void GLUTMouse(int button, int state, int x, int y)
{
    // Invert y coordinate
    y = GLUTwindow_height - y;
    
    // Process mouse button event
    if (state == GLUT_DOWN) {
        if (button == GLUT_LEFT_BUTTON) {
        }
        else if (button == GLUT_MIDDLE_BUTTON) {
        }
        else if (button == GLUT_RIGHT_BUTTON) {
        }
    }
    
    // Remember button state
    int b = (button == GLUT_LEFT_BUTTON) ? 0 : ((button == GLUT_MIDDLE_BUTTON) ? 1 : 2);
    GLUTbutton[b] = (state == GLUT_DOWN) ? 1 : 0;
    
    // Remember modifiers
    GLUTmodifiers = glutGetModifiers();
    
    // Remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;
    
    // Redraw
    glutPostRedisplay();
}



void GLUTSpecial(int key, int x, int y)
{
    // Invert y coordinate
    y = GLUTwindow_height - y;
    
    // Process keyboard button event
    switch (key) {
        case GLUT_KEY_F1:
            save_image = 1;
            break;
        case GLUT_KEY_F2:
            save_video = save_video ^ 1;
            break;
    }
    
    // Remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;
    
    // Remember modifiers
    GLUTmodifiers = glutGetModifiers();
    
    // Redraw
    glutPostRedisplay();
}


void keyboard()
{
    double rotateAmount = 0.020;
    
    
    //boooooooooost
    if (keyStates['H'] || keyStates['h']) {
        //ran out of boost but still pressing h
        if (scene->players[0]->boost <= 10) {
            scene->players[0]->boost = 0;
            scene->players[0]->velocity = max(scene->players[0]->defaultVelocity, scene->players[0]->velocity * .95);
            scene->players[0]->accel = false;
        }
        //boosting
        else if (scene->players[0]->accel) {
            scene->players[0]->boost -= 0.5;
            scene->players[0]->velocity = min(4*scene->players[0]->defaultVelocity, scene->players[0]->velocity * 1.5);
            
            
            timeval current_time;
            gettimeofday(&current_time, NULL);
            double ellapsedTime = (current_time.tv_sec - last_boost_time.tv_sec) * 1000.0;
            ellapsedTime += (current_time.tv_usec - last_boost_time.tv_usec) / 1000.0;
            if (ellapsedTime > 2000) {
                gettimeofday(&last_boost_time, NULL);
                pid_t pid;
                pid = fork();
                if (pid == 0) {
                    if (LINUX)
                        system("avplay -nodisp -autoexit Comet.wav");
                    else
                        system("afplay Comet.wav");
                    exit(0);
                }
            }

        }
        else
            scene->players[0]->boost = min(scene->players[0]->boost + .1, (double)100);
        
    }
    else { //regaining boost power
        scene->players[0]->boost = min(scene->players[0]->boost + .25, (double)100);
        scene->players[0]->velocity = max(scene->players[0]->defaultVelocity, scene->players[0]->velocity * .95);
        if (scene->players[0]->boost == 100)
            scene->players[0]->accel = true;
    }
	if (scene->players.size() != 0) {
		if (keyStates['W'] || keyStates['w']){
			scene->players[0]->shape->mesh->Rotate(1.0 * rotateAmount, R3Line(scene->players[0]->pos, scene->players[0]->wing));
			scene->players[0]->nose.Rotate(scene->players[0]->wing, 1.0 * rotateAmount);
		}
        
		//shoot
		if (keyStates['G'] || keyStates['g']){
            if (scene->players[0]->missiles <= 0)
                scene->players[0]->currentbullet = R3_REGULAR_BULLET;
            ShootBullet(scene);
            
		}
        
		if (keyStates['S'] || keyStates['s']){
			scene->players[0]->shape->mesh->Rotate(-1.0 * rotateAmount, R3Line(scene->players[0]->pos, scene->players[0]->wing));
			scene->players[0]->nose.Rotate(scene->players[0]->wing, -1.0 * rotateAmount);
		}
		
		if (keyStates['D'] || keyStates['d']){
			scene->players[0]->shape->mesh->Rotate(1.0 * rotateAmount, R3Line(scene->players[0]->pos, scene->players[0]->nose));
			scene->players[0]->wing.Rotate(scene->players[0]->nose, 1.0 * rotateAmount);
		}
		if (keyStates['A'] || keyStates['a']){
			scene->players[0]->shape->mesh->Rotate(-1.0 * rotateAmount, R3Line(scene->players[0]->pos, scene->players[0]->nose));
			scene->players[0]->wing.Rotate(scene->players[0]->nose, -1.0 * rotateAmount);
		}
	}
    
}

void keyUp (unsigned char key, int x, int y) {
    keyStates[key] = false; // Set the state of the current key to not pressed
    /*if (key == 'g') {
      fprintf(stderr, "kill bsound %d", BSound);
      kill(BSound, SIGKILL);
      kill(BSound+1, SIGKILL);
      kill(BSound+2, SIGKILL);
      BSound = -1;
      }*/
}



void GLUTKeyboard(unsigned char key, int x, int y)
{
    // Invert y coordinate
    y = GLUTwindow_height - y;
    
    // Process keyboard button event
    switch (key) {
            
            
            
        case 'W':
        case 'w':
            keyStates['w'] = true;
            break;
            
        case 'Y':
        case 'y':
            
            // bullet toggle
            if (scene->players[0]->currentbullet == R3_REGULAR_BULLET && scene->players[0]->missiles != 0) {
                scene->players[0]->currentbullet = R3_MISSILE_BULLET;
            }
            else if (scene->players[0]->currentbullet == R3_MISSILE_BULLET) {
                scene->players[0]->currentbullet = R3_REGULAR_BULLET;
            }
//            else {
  //              fprintf(stderr,"Why do you not have a bullettype?");
    //        }
            break;
            
        case 'S':
        case 's':
            keyStates['s'] = true;
            
            break;
            
        case 'D':
        case 'd':
            keyStates['d'] = true;
            
            break;
            
        case 'A':
        case 'a':
            keyStates['a'] = true;
            break;
            
        case 'G':
        case 'g':
            keyStates['g'] = true;
            break;
        
        case 'H':
        case 'h':
            keyStates['h'] = true;
            break;
            
            
        case 'M':
        case 'm':
            scene->players[0]->missiles += 10;
            scene->players[0]->boidsKilled += 50;
            break;
            
            
            //boid test code
        case 'B':
        case 'b':
            view3 = !view3;
            if (view3) {
                view2 = false;
                follow = false;
            }
            else
                view2 = true;
            break;
            
        case 'X':
        case 'x':
            follow = !follow;
            if (follow) {
                view2 = false;
                view3 = false; 
            }
            break;
            
        case 'Z':
        case 'z':
            view2 = !view2;
            if (view2) {
                follow = false;
                view3 = false; 
            }
            break;

            /*
             case 'S':
             case 's':
             show_particle_sources_and_sinks = !show_particle_sources_and_sinks;
             break; */
            
    case 'k':
      useShader=!useShader;
      cout<<"\rShader is "<<(useShader?"on ":"off")<<flush;
      if(useShader) {
	glUseProgram(shader);
	UniformLocation = glGetUniformLocation(shader, "texton");
      }
      else
	glUseProgram(0);
      break;
    

            
        case ' ': {
            printf("camera %g %g %g  %g %g %g  %g %g %g  %g  %g %g \n",
                   camera.eye[0], camera.eye[1], camera.eye[2],
                   camera.towards[0], camera.towards[1], camera.towards[2],
                   camera.up[0], camera.up[1], camera.up[2],
                   camera.xfov, camera.neardist, camera.fardist);
            break; }
    }
    
    // Remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;
    
    // Remember modifiers
    GLUTmodifiers = glutGetModifiers();
    
    // Redraw
    glutPostRedisplay();
}



void GLUTCommand(int cmd)
{
    // Execute command
    switch (cmd) {
        case DISPLAY_PARTICLES_TOGGLE_COMMAND: show_particles = !show_particles; break;
        case DISPLAY_PARTICLE_SPRINGS_TOGGLE_COMMAND: show_particle_springs = !show_particle_springs; break;
        case DISPLAY_PARTICLE_SOURCES_AND_SINKS_TOGGLE_COMMAND: show_particle_sources_and_sinks = !show_particle_sources_and_sinks; break;
        case DISPLAY_FACE_TOGGLE_COMMAND: show_faces = !show_faces; break;
        case DISPLAY_EDGE_TOGGLE_COMMAND: show_edges = !show_edges; break;
        case DISPLAY_BBOXES_TOGGLE_COMMAND: show_bboxes = !show_bboxes; break;
        case DISPLAY_LIGHTS_TOGGLE_COMMAND: show_lights = !show_lights; break;
        case DISPLAY_CAMERA_TOGGLE_COMMAND: show_camera = !show_camera; break;
        case SAVE_IMAGE_COMMAND: save_image = 1; break;
        case SAVE_VIDEO_COMMAND: save_video = save_video ^ 1; break;
        case QUIT_COMMAND: quit = 1; break;
    }
    
    // Mark window for redraw
    glutPostRedisplay();
}



void GLUTCreateMenu(void)
{
    
    // Attach main menu to right mouse button
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}



void GLUTInit(int *argc, char **argv)
{
    // Open window
    glutInit(argc, argv);
    glutInitWindowPosition(100, 100);
    
    glutInitWindowSize(GLUTwindow_width, GLUTwindow_height);
    
//    glutInitWindowSize(2*GLUTwindow_width, 2*GLUTwindow_height);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // | GLUT_STENCIL
    GLUTwindow = glutCreateWindow("OpenGL Viewer");
    
    // Initialize GLUT callback functions
    glutIdleFunc(GLUTIdle);
    glutReshapeFunc(GLUTResize);
    glutDisplayFunc(GLUTRedraw);
    glutKeyboardFunc(GLUTKeyboard);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(GLUTSpecial);
    glutMouseFunc(GLUTMouse);
    glutMotionFunc(GLUTMotion);
    
    // Initialize graphics modes
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    
    // Create menus
    GLUTCreateMenu();
}




////////////////////////////////////////////////////////////
// SCENE READING
////////////////////////////////////////////////////////////


R3Scene *
ReadScene(const char *filename)
{
    // Allocate scene
    R3Scene *scene = new R3Scene();
    if (!scene) {
        fprintf(stderr, "Unable to allocate scene\n");
        return NULL;
    }
    
    // Read file
    if (!scene->Read(filename)) {
        fprintf(stderr, "Unable to read scene from %s\n", filename);
        return NULL;
    }
    
    // Remember initial camera
    camera = scene->camera;
    
    //generate first set of opponent boids
    GenerateBoids(scene, 5, 40.);
    
    // Return scene
    return scene;
}



////////////////////////////////////////////////////////////
// PROGRAM ARGUMENT PARSING
////////////////////////////////////////////////////////////

int
ParseArgs(int argc, char **argv)
{
    // Innocent until proven guilty
    int print_usage = 0;
    
    // Parse arguments
    argc--; argv++;
    while (argc > 0) {
        if ((*argv)[0] == '-') {
            if (!strcmp(*argv, "-help")) { print_usage = 1; }
            else if (!strcmp(*argv, "-exit_immediately")) { quit = 1; }
            else if (!strcmp(*argv, "-output_image")) { argc--; argv++; output_image_name = *argv; }
            else if (!strcmp(*argv, "-video_prefix")) { argc--; argv++; video_prefix = *argv; }
            else if (!strcmp(*argv, "-euler")) integration_type = EULER_INTEGRATION;
            else if (!strcmp(*argv, "-midpoint")) integration_type = MIDPOINT_INTEGRATION;
            else if (!strcmp(*argv, "-rk4")) integration_type = RK4_INTEGRATION;
            else if (!strcmp(*argv, "-adaptive_step_size")) integration_type = ADAPTIVE_STEP_SIZE_INTEGRATION;
            else if (!strcmp(*argv, "-recordandquit")) {
                argc--; argv++; num_frames_to_record = atoi(*argv);
                GLUTwindow_width = 256 * 1.6;
                GLUTwindow_height = 256 * 1.6;
                save_video = 1;
            }
            else { fprintf(stderr, "Invalid program argument: %s", *argv); exit(1); }
            argv++; argc--;
        }
        else {
            if (!input_scene_name) input_scene_name = *argv;
            else { fprintf(stderr, "Invalid program argument: %s", *argv); exit(1); }
            argv++; argc--;
        }
    }
    
    // Check input_scene_name
    if (!input_scene_name || print_usage) {
        printf("Usage: particleview <input.scn> [-exit_immediately] [-output_image OUTPUT.JPG]  [-video_prefix VIDEO_DIR/PREFIX_] [-euler] [-midpoint] [-rk4] [-adaptive_step_size]  [-recordandquit NUM_FRAMES] [-v]\n");
        return 0;
    }
    
    // Return OK status
    return 1;
}


// set the toon shader stuff
GLuint setShaders() {

  char *vs,*fs;

  v = glCreateShader(GL_VERTEX_SHADER);
  f = glCreateShader(GL_FRAGMENT_SHADER);


  vs = textFileRead("src/toon.vert");
  fs = textFileRead("src/toon.frag");

  if (!vs || !fs) {
    vs = textFileRead("./toon.vert");
    fs = textFileRead("./toon.frag");
  }

  if(vs && fs)
    {

      const char * vv = vs;
      const char * ff = fs;

      glShaderSource(v, 1, &vv,NULL);
      glShaderSource(f, 1, &ff,NULL);
      free(vs);free(fs);

      glCompileShader(v);
      printShaderInfoLog(v);
      glCompileShader(f);
      printShaderInfoLog(f);
      GLuint p = glCreateProgram();

      glAttachShader(p,v);
      glAttachShader(p,f);
      glLinkProgram(p);
      printProgramInfoLog(p);
      //glUseProgram(p); --> let's not rush with using the shader right away                                                                                             
      //see processNormalKeys for turning shader on and off                                                                                                          
      return p;
    }
  cerr<<"Could not read the shader source"<<endl;
  return 0;
}


////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////

void setUpNewLevel(R3Scene *scene) {
    scene = ReadScene(input_scene_name);
}

int
main(int argc, char **argv)
{

  gettimeofday(&last_boost_time, NULL);



  for (int i = 0; i < 256; i++) {
        keyStates[i] = false;
  }
    // Parse program arguments
    if (!ParseArgs(argc, argv)) exit(1);
    
    // Initialize GLUT
    GLUTInit(&argc, argv);
    
    // Read scene
    scene = ReadScene(input_scene_name);
    if (!scene) exit(-1);

    if (scene->enemies.size() != 0 && scene->enemies[0]->shape->type == R3_MESH_SHAPE) {
      scene->enemies[0]->shape->mesh->Translate(50, 0, -120);
      scene->enemies[0]->shape->mesh->Rotate(3 * M_PI / 2, R3Line(scene->enemies[0]->shape->mesh->Center(), R3Vector(1, 0, 0)));
      scene->enemies[0]->direction = R3Vector(0, -1, 0);
      scene->enemies[0]->shape->mesh->Scale(0.50, 0.50, 0.50);
    }
    
    float versionGL = initGlew(true);
    if(versionGL!=0.)
      cout<<"Glew ready to go with OpenGL "<<versionGL<<"\n==================="<<endl;
    else
      {
	cout<<"Glew failed to initialize \n==================="<<endl;
	exit(-1);
      }

    shader=setShaders();
    cout<<"Shader program "<<shader<<endl;

    glShadeModel (GL_SMOOTH);

    // Run GLUT interface
    GLUTMainLoop();
    
    // Return success 
    return 0;
}







