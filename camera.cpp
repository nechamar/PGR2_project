#include "camera.h"
#include "handler.h"
#include <math.h>

const glm::vec3& camera::getPosition() const {
    return position;
}

void camera::setPosition(glm::vec3 position) {
    camera::position = position;
}

const glm::vec3& camera::getDirection() const {
    return direction;
}

void camera::setDirection(glm::vec3 direction) {
    camera::direction = direction;
}

const glm::vec3& camera::getUpVector() const {
    return upVector;
}

void camera::setUpVector(glm::vec3 upVector) {
    camera::upVector = upVector;
}

void camera::update(double time) {
    /// acceleration of camera, which makes camera movement slower or faster
    float acceleration = time;
    /// if shift is pressed camera movement is faster
    if (handler.specKeys[keys::shift]) { acceleration *= 5; }
    /// if w, s, a or d is pressed camera moves in standard direction of the key
    if (handler.keys['w']) position = position + acceleration * direction;
    if (handler.keys['s']) position = position - acceleration * direction;
    if (handler.keys['a']) position = position - acceleration * glm::cross(direction, upVector);
    if (handler.keys['d']) position = position + acceleration * glm::cross(direction, upVector);
    //if (handler.specKeys[GLUT_KEY_UP]) position = position + acceleration * upVector;
    //if (handler.specKeys[GLUT_KEY_DOWN]) position = position - acceleration * upVector;

    /// rotates view of camera in horizontal way
    direction = glm::mat3(glm::rotate(glm::mat4(1.0f), (float)handler.mouseDx/500, getUpVector())) * direction;
    /// vector to rotate around when rotating vertically
    glm::vec3 toRotateAround = glm::cross(direction, upVector);

    /// remembers direction before changes if there are some problems
    glm::vec3 oldDirection = direction;
    /// rotates view of camera in vertical way
    direction = glm::mat3(glm::rotate(glm::mat4(1.0f), (float)handler.mouseDy/500, toRotateAround)) * direction;
    /// user has limited view (can't bend back to see behind himself using vertex rotation)
    /// if any problem with rotations, direction is returned to the value before rotation
    if (direction.y > 0.9 || direction.y < -0.9 || (handler.mouseDy > 0 && direction.y < oldDirection.y) || (handler.mouseDy < 0 && direction.y > oldDirection.y)) direction = oldDirection;

    /// set mouse pointer to the window center
    //glutWarpPointer(handler.windowWidth / 2, handler.windowHeight / 2);
}
