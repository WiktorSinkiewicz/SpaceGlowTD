#include "Camera.h"

Camera::Camera(glm::vec3 position) 
    : WorldUp(glm::vec3(0.0f, 1.0f, 0.0f)), Yaw(YAW), Pitch(PITCH), MovementSpeed(SPEED), Zoom(ZOOM)
{
    Position = position;
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
    float velocity = MovementSpeed * deltaTime;
    // Movement on XZ plane only
    
    glm::vec3 flatFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
    glm::vec3 rightDir = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

    if (direction == FORWARD)
        Position += flatFront * velocity;
    if (direction == BACKWARD)
        Position -= flatFront * velocity;
    if (direction == LEFT)
        Position -= rightDir * velocity;
    if (direction == RIGHT)
        Position += rightDir * velocity;
    if (direction == ZOOM_IN)
        Position += Front * velocity * 2.0f;
    if (direction == ZOOM_OUT)
        Position -= Front * velocity * 2.0f;
        
    // Clamp height
    if (Position.y < 2.0f) Position.y = 2.0f;
    if (Position.y > 45.0f) Position.y = 45.0f;
}

void Camera::ProcessMouseScroll(float yoffset) {
    Position += Front * yoffset * 2.0f;
    
    // Clamp height
    if (Position.y < 2.0f) Position.y = 2.0f;
    if (Position.y > 45.0f) Position.y = 45.0f;
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}
