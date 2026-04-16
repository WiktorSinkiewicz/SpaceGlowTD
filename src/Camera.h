#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Kierunki przesuwania kamery po płaskim terenie
enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

// Stałe RTS dla widoku statycznego
const float YAW         = -90.0f;
const float PITCH       = -60.0f; // Stały, stromy kąt w dół
const float SPEED       = 10.0f;  // Szybsze przesuwanie na szerokiej mapie
const float ZOOM        = 45.0f;

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;
    float MovementSpeed;
    float Zoom;

    Camera(glm::vec3 position = glm::vec3(0.0f, 15.0f, 10.0f));

    glm::mat4 GetViewMatrix() const;
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);

private:
    void updateCameraVectors();
};
