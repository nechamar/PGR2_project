#pragma once
#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/gtx/transform.hpp"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

struct material {
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    glm::vec3 emission;
    float shininess;

    bool useEmissionTexture;
    GLuint emissionTexture[2];
};

struct modelGeometry {
    GLuint elementBufferObject;
    GLuint vertexArrayObject;
    GLuint vertexBufferObject;
    unsigned int numTriangles;
    material meshMaterial;
};

enum keys {
    shift = 112,
    escape = 27,
    space = 32,
    tab = 9
};

struct Handler {

GLFWwindow* window;
GLFWwindow* bufferContextWindow;
GLFWwindow* textureContextWindow;


/// program location for entities
GLuint program;
/// uniform locations for entities
GLint pvmMatrix;
GLint vmMatrix;
GLint mMatrix;
GLint vMatrix;
GLint nMatrix;
/// attribute locations for entities
GLint position;
GLint normal;
/// uniform texture locations for entities
GLint useEmissionTexture;
/// key maps for normal and special keys
bool* keys;
bool* specKeys;
/// mouse positions and change of mouse position since last update of entity manager
double mouseX;
double mouseY;
int mouseDx;
int mouseDy;
/// width and height of window
int windowWidth;
int windowHeight;

// models
static const unsigned char nModels = 2;
modelGeometry models[nModels];
static const unsigned char nTextures = 6;
unsigned char* textures[nTextures]; // in cpu memory
GLuint GPUtextures[nTextures];      // in gpu memory
int widths[nTextures];
int heights[nTextures];

// using asnyc texture loading
bool async = false;

};

extern Handler handler;