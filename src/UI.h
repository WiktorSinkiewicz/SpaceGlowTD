#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

#include "Entities.h"
#include "Map.h"
#include "Camera.h"

// Rysuje cały interfejs ImGui (HUD, panel budowy, menu wieży, pauza, game over)
void renderUI(GLFWwindow* window, int fbW, int fbH, float aspect,
              Camera& camera,
              std::vector<Enemy>& enemies,
              std::vector<Tower>& placedTowers,
              Map& levelMap);
