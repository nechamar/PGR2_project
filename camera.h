#pragma once
#include "glm/glm.hpp"

class camera
{
protected:
    /// vector of three floats, which provides x,y and z position of camera in world coordinates.
    glm::vec3 position;
    /// unit vector of three floats, which provides x, y and z direction of camera with respect to position of camera.
    glm::vec3 direction;
    /// unit vector of three float, which provides x, y and z of vector pointing up with respect to position of camera.
    glm::vec3 upVector;

public:

    /// setters and getters for every class variable.
    const glm::vec3& getPosition() const;

    void setPosition(glm::vec3 position);

    const glm::vec3& getDirection() const;

    void setDirection(glm::vec3 direction);

    const glm::vec3& getUpVector() const;

    void setUpVector(glm::vec3 upVector);
    /// method called in every time scene is updated if this camera is main camera. It should manage class variables
    void update(double time);

};

