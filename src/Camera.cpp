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
    // Blokujemy płaszczyznę logiki. Klawisze przesuwają kamerę wzdłuż X i Z.
    // Aby lot góra/dół klawiszami W i S był wzdłuż podłogi Z, spłaszczamy wektor kierunku na Y = 0.
    
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
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch)); // Y zawsze wymuszony by patrzeć w dół pod stałym PITCH
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}
