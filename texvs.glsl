#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoords;
layout (location = 2) in vec3 normal;

smooth out vec2 o_texCoords;
smooth out vec3 o_normal;
smooth out vec3 o_position;

uniform mat4 pvmMatrix;
uniform mat4 vmMatrix;
uniform mat4 mMatrix;
uniform mat4 vMatrix;
uniform mat3 nMatrix;


uniform bool useEmissionTexture;

void main() {

    gl_Position = pvmMatrix * vec4(position, 1.0);
    vec3 norm = normalize((vMatrix * vec4(nMatrix * normal, 0.0f)).xyz);
    vec3 pos = (vmMatrix * vec4(position, 1.0)).xyz;

    o_texCoords = texCoords;
    o_normal = norm;
    o_position = pos;
}
