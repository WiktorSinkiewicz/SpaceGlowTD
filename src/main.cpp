#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "Camera.h"
#include "Entities.h"
#include "GameState.h"
#include "Geometry.h"
#include "Map.h"
#include "Shader.h"
#include "UI.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(glm::vec3(10.0f, 15.0f, 22.0f));

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Game state definitions (declared in GameState.h)
GameConfig cfg;
int playerHP = cfg.initialPlayerHP;
int materials = cfg.initialMaterials;
int score = 0;
bool gameOver = false;
bool gamePaused = false;
int currentWave = 1;
int enemiesSpawnedInWave = 0;
float waveTimer = cfg.timeBetweenWaves;
bool waveInProgress = false;
int selectedTowerBuild = -1;
int selectedPlacedTowerIndex = -1;

static int nextEnemyId = 1;

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(
      SCR_WIDTH, SCR_HEIGHT, "SpaceGlowTD - Voxel Terrain", NULL, NULL);
  if (window == NULL) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetScrollCallback(window, scroll_callback);

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  glEnable(GL_DEPTH_TEST);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  // ImGui init
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  Shader basicShader("shaders/basic.vert", "shaders/basic.frag");

  unsigned int VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(BOX_VERTICES), BOX_VERTICES,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  GLuint towerVAO[3], towerVBO[3];
  int towerVertexCount[3];
  int sides[3] = {4, 3, 6};
  for (int i = 0; i < 3; i++) {
    std::vector<float> verts = generatePrismVertices(sides[i]);
    towerVertexCount[i] = verts.size() / 6;
    glGenVertexArrays(1, &towerVAO[i]);
    glGenBuffers(1, &towerVBO[i]);
    glBindVertexArray(towerVAO[i]);
    glBindBuffer(GL_ARRAY_BUFFER, towerVBO[i]);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
  }

  GLuint rangeVAO, rangeVBO;
  glGenVertexArrays(1, &rangeVAO);
  glGenBuffers(1, &rangeVBO);
  glBindVertexArray(rangeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, rangeVBO);

  glBufferData(GL_ARRAY_BUFFER, 128 * 3 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  std::vector<Tower> placedTowers;

  Map levelMap(20, 20);
  levelMap.generateTerrain();
  levelMap.generateWindingPath(40, 52);

  std::vector<Enemy> enemies;
  std::vector<Projectile> projectiles;
  std::vector<Particle> particles;
  std::vector<FlashLight> flashes;
  float spawnTimer = 0.0f;

  while (!glfwWindowShouldClose(window)) {
    int fbW = (int)SCR_WIDTH;
    int fbH = (int)SCR_HEIGHT;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbW < 1)
      fbW = (int)SCR_WIDTH;
    if (fbH < 1)
      fbH = (int)SCR_HEIGHT;
    const float aspect = (float)fbW / (float)std::max(1, fbH);

    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    if (deltaTime > 0.1f)
      deltaTime = 0.1f;

    static bool escPrev = false;
    const bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escDown && !escPrev)
      gamePaused = !gamePaused;
    escPrev = escDown;

    static bool wasPaused = false;
    if (wasPaused && !gamePaused)
      lastFrame = glfwGetTime();
    wasPaused = gamePaused;

    processInput(window);

    // Raycasting
    int hoverX = -1;
    int hoverZ = -1;
    bool hoverValid = false;

    if (!ImGui::GetIO().WantCaptureMouse) {
      double mx, my;
      glfwGetCursorPos(window, &mx, &my);
      float ndcX = (2.0f * mx) / fbW - 1.0f;
      float ndcY = 1.0f - (2.0f * my) / fbH;

      glm::mat4 proj =
          glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 100.0f);
      glm::mat4 viewM = camera.GetViewMatrix();
      glm::vec4 clipCoords(ndcX, ndcY, -1.0f, 1.0f);
      glm::vec4 eyeCoords = glm::inverse(proj) * clipCoords;
      eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);
      glm::vec3 rayWorld =
          glm::normalize(glm::vec3(glm::inverse(viewM) * eyeCoords));

      float closestT = 99999.0f;
      for (int x = 0; x < levelMap.width; x++) {
        for (int z = 0; z < levelMap.height; z++) {
          Tile &t = levelMap.grid[x][z];
          glm::vec3 boxMin(x * 1.0f - 0.475f, 0.0f, z * 1.0f - 0.475f);
          glm::vec3 boxMax(x * 1.0f + 0.475f, t.height, z * 1.0f + 0.475f);
          float tHit;
          if (intersectRayAABB(camera.Position, rayWorld, boxMin, boxMax,
                               tHit)) {
            if (tHit < closestT) {
              closestT = tHit;
              hoverX = x;
              hoverZ = z;
              hoverValid = true;
            }
          }
        }
      }
    }

    static bool mouseLeftPrev = false;
    bool mouseLeftDown =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (mouseLeftDown && !mouseLeftPrev && hoverValid && !gamePaused &&
        !gameOver && !ImGui::GetIO().WantCaptureMouse) {
      Tile &t = levelMap.grid[hoverX][hoverZ];
      if (selectedTowerBuild != -1 && t.type == TILE_EMPTY && !t.hasTower) {
        int cost = (selectedTowerBuild == 0)
                       ? cfg.towerBasicCost
                       : ((selectedTowerBuild == 1) ? cfg.towerLaserCost
                                                    : cfg.towerMortarCost);
        if (materials >= cost) {
          materials -= cost;
          t.hasTower = true;
          placedTowers.push_back({hoverX, hoverZ, selectedTowerBuild, 0.0f, -1,
                                  0.0f, glm::vec3(0.0f), false, 0});
          selectedTowerBuild = -1;
        }
      } else if (selectedTowerBuild == -1 && t.hasTower) {
        // Select clicked tower
        selectedPlacedTowerIndex = -1;
        for (size_t i = 0; i < placedTowers.size(); i++) {
          if (placedTowers[i].x == hoverX && placedTowers[i].z == hoverZ) {
            selectedPlacedTowerIndex = (int)i;
            break;
          }
        }
      } else if (selectedTowerBuild == -1 && !t.hasTower) {
        // Click on empty = deselect
        selectedPlacedTowerIndex = -1;
      }
    }
    mouseLeftPrev = mouseLeftDown;

    static bool mouseRightPrev = false;
    bool mouseRightDown =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (mouseRightDown && !mouseRightPrev) {
      selectedTowerBuild = -1;
      selectedPlacedTowerIndex = -1;
    }
    mouseRightPrev = mouseRightDown;

    // Game logic paused during game over or pause
    if (!gameOver && !gamePaused) {

      // Wave spawning system
      if (waveInProgress) {
        spawnTimer += deltaTime;
        if (spawnTimer >= cfg.spawnRate &&
            enemiesSpawnedInWave < cfg.enemiesPerWave) {
          spawnTimer = 0.0f;
          if (levelMap.path.size() > 1) {
            Enemy e;
            e.id = nextEnemyId++;
            e.pathIndex = 0;
            e.progress = 0.0f;
            e.currentPos =
                glm::vec3(levelMap.path[0].x, 0.4f, levelMap.path[0].y);

            // Scale HP per wave
            e.maxHp = cfg.initialEnemyHP *
                      std::pow(cfg.enemyHPMultiplierPerWave, currentWave - 1);
            e.hp = e.maxHp;
            e.reward = cfg.initialEnemyReward +
                       (currentWave - 1) * cfg.rewardIncreasePerWave;

            enemies.push_back(e);
            enemiesSpawnedInWave++;
          }
        } else if (enemiesSpawnedInWave >= cfg.enemiesPerWave &&
                   enemies.empty()) {
          // Wave complete
          waveInProgress = false;
          currentWave++;
          enemiesSpawnedInWave = 0;
          waveTimer = cfg.timeBetweenWaves;
        }
      } else {
        // Waiting for next wave
        waveTimer -= deltaTime;
        if (waveTimer <= 0.0f) {
          waveInProgress = true;
          spawnTimer = cfg.spawnRate; // Spawn first enemy immediately
        }
      }

      // --------------------------------
      // Tower firing logic
      // --------------------------------
      for (Tower &tow : placedTowers) {
        tow.cooldownTimer -= deltaTime;

        if (tow.type == 0) { // Basic Tower
          if (tow.cooldownTimer <= 0.0f) {
            float actualRange = cfg.towerBasicRange *
                                (1.0f + levelMap.grid[tow.x][tow.z].height *
                                            cfg.heightRangeMultiplier);
            float closestDistSq = actualRange * actualRange;
            int targetId = -1;
            glm::vec3 towerPos(tow.x * 1.0f,
                               levelMap.grid[tow.x][tow.z].height + 1.2f,
                               tow.z * 1.0f);

            for (const Enemy &e : enemies) {
              float dx = towerPos.x - e.currentPos.x;
              float dy = towerPos.y - e.currentPos.y;
              float dz = towerPos.z - e.currentPos.z;
              float distSq = dx * dx + dy * dy + dz * dz;

              if (distSq < closestDistSq) {
                closestDistSq = distSq;
                targetId = e.id;
              }
            }

            if (targetId != -1) {
              Projectile p;
              p.pos = towerPos;
              p.targetId = targetId;
              p.damage =
                  cfg.towerBasicDamage * (1.0f + tow.upgradeLevel * 1.0f);
              p.color = glm::vec3(0.1f, 1.0f, 0.9f);
              projectiles.push_back(p);
              tow.cooldownTimer = cfg.towerBasicCooldown;
            }
          }
        } else if (tow.type == 1) { // Laser Tower
          float actualRange =
              cfg.towerLaserRange * (1.0f + levelMap.grid[tow.x][tow.z].height *
                                                cfg.heightRangeMultiplier);
          glm::vec3 towerPos(tow.x * 1.0f,
                             levelMap.grid[tow.x][tow.z].height + 1.2f,
                             tow.z * 1.0f);

          tow.isFiringLaser = false;

          Enemy *target = nullptr;

          if (tow.lockedTargetId != -1) {
            for (Enemy &e : enemies) {
              if (e.id == tow.lockedTargetId) {
                target = &e;
                break;
              }
            }
          }

          if (!target ||
              glm::distance(towerPos, target->currentPos) > actualRange) {
            tow.lockedTargetId = -1;
            tow.lockTime = 0.0f;
            target = nullptr;

            float closestDist = actualRange;
            for (Enemy &e : enemies) {
              float dist = glm::distance(towerPos, e.currentPos);
              if (dist <= closestDist) {
                closestDist = dist;
                target = &e;
                tow.lockedTargetId = e.id;
              }
            }
          }

          if (target) {
            glm::vec3 targetPos =
                target->currentPos + glm::vec3(0.0f, 0.4f, 0.0f);
            float dist = glm::distance(towerPos, targetPos);
            glm::vec3 dir = glm::normalize(targetPos - towerPos);

            bool blocked = false;
            float step = 0.2f;
            for (float d = 0.0f; d < dist; d += step) {
              glm::vec3 p = towerPos + dir * d;
              int gx = std::clamp((int)std::round(p.x), 0, levelMap.width - 1);
              int gz = std::clamp((int)std::round(p.z), 0, levelMap.height - 1);
              if (levelMap.grid[gx][gz].height > p.y) {
                tow.laserHitPos = p;
                tow.laserHitPos.y = levelMap.grid[gx][gz].height + 0.1f;
                blocked = true;
                break;
              }
            }

            if (blocked) {
              tow.isFiringLaser = true;
              tow.lockedTargetId = -1;
              tow.lockTime = 0.0f;
            } else {
              tow.laserHitPos = targetPos;
              tow.isFiringLaser = true;
              tow.lockTime += deltaTime;

              float damageScale =
                  1.0f + (tow.lockTime / cfg.towerLaserChargeTime) *
                             (cfg.towerLaserDamageMaxMultiplier - 1.0f);
              damageScale =
                  std::min(damageScale, cfg.towerLaserDamageMaxMultiplier);

              target->hp -= cfg.towerLaserDamageBase * damageScale *
                            (1.0f + tow.upgradeLevel * 1.0f) * deltaTime;
            }
          }
        } else if (tow.type == 2) { // Mortar Tower
          float actualRange = cfg.towerMortarRange *
                              (1.0f + levelMap.grid[tow.x][tow.z].height *
                                          cfg.heightRangeMultiplier);
          glm::vec3 towerPos(tow.x * 1.0f,
                             levelMap.grid[tow.x][tow.z].height + 1.2f,
                             tow.z * 1.0f);

          if (tow.cooldownTimer <= 0.0f) {
            float closestDistSq = actualRange * actualRange;
            Enemy *target = nullptr;

            for (Enemy &e : enemies) {
              float dx = towerPos.x - e.currentPos.x;
              float dy = towerPos.y - e.currentPos.y;
              float dz = towerPos.z - e.currentPos.z;
              float distSq = dx * dx + dy * dy + dz * dz;

              if (distSq < closestDistSq) {
                closestDistSq = distSq;
                target = &e;
              }
            }

            if (target) {
              glm::vec2 startXZ(towerPos.x, towerPos.z);
              glm::vec2 targetXZ(target->currentPos.x, target->currentPos.z);
              float initialDistXZ = glm::distance(startXZ, targetXZ);

              float estimatedFlightTime =
                  initialDistXZ / cfg.mortarHorizontalSpeed;
              float futureProgress =
                  target->progress + cfg.enemySpeed * estimatedFlightTime;
              int futurePathIndex = target->pathIndex;
              while (futureProgress >= 1.0f &&
                     futurePathIndex < (int)levelMap.path.size() - 2) {
                futureProgress -= 1.0f;
                futurePathIndex++;
              }

              glm::vec2 p1 = glm::vec2((float)levelMap.path[futurePathIndex].x,
                                       (float)levelMap.path[futurePathIndex].y);
              glm::vec2 p2 =
                  glm::vec2((float)levelMap.path[futurePathIndex + 1].x,
                            (float)levelMap.path[futurePathIndex + 1].y);
              glm::vec2 predictedXZ = glm::mix(p1, p2, futureProgress);
              glm::vec3 predictedPos =
                  glm::vec3(predictedXZ.x, target->currentPos.y, predictedXZ.y);

              float finalDistXZ = glm::distance(startXZ, predictedXZ);

              if (finalDistXZ > 0.1f) {
                float flightTime = finalDistXZ / cfg.mortarHorizontalSpeed;
                float heightDiff = predictedPos.y - towerPos.y;
                float v0y = (heightDiff + 0.5f * cfg.mortarGravity *
                                              flightTime * flightTime) /
                            flightTime;
                glm::vec2 dirXZ = glm::normalize(predictedXZ - startXZ) *
                                  cfg.mortarHorizontalSpeed;

                Projectile p;
                p.type = 1;
                p.pos = towerPos;
                p.velocity = glm::vec3(dirXZ.x, v0y, dirXZ.y);
                p.targetId = -1;
                p.damage =
                    cfg.towerMortarDamage * (1.0f + tow.upgradeLevel * 1.0f);
                p.color = glm::vec3(1.0f, 0.5f, 0.0f);
                projectiles.push_back(p);

                tow.cooldownTimer = cfg.towerMortarCooldown;
              }
            }
          }
        }
      }

      // Projectile physics
      for (size_t i = 0; i < projectiles.size(); i++) {
        Projectile &p = projectiles[i];

        if (p.type == 0) {
          // Basic Homing
          Enemy *target = nullptr;
          for (Enemy &e : enemies) {
            if (e.id == p.targetId) {
              target = &e;
              break;
            }
          }

          if (target) {
            glm::vec3 aimPos = target->currentPos + glm::vec3(0.0f, 0.4f, 0.0f);
            glm::vec3 dir = glm::normalize(aimPos - p.pos);
            p.pos += dir * cfg.projectileSpeed * deltaTime;

            // Hit detection
            if (glm::distance(p.pos, aimPos) < 0.5f) {
              target->hp -= p.damage;
              projectiles.erase(projectiles.begin() + i);
              i--;
              continue;
            }
          } else {
            // Target died, projectile falls
            p.pos.y -= cfg.projectileSpeed * 0.5f * deltaTime;
          }

          // Terrain collision
          int gridX =
              std::clamp((int)std::round(p.pos.x), 0, levelMap.width - 1);
          int gridZ =
              std::clamp((int)std::round(p.pos.z), 0, levelMap.height - 1);
          if (p.pos.y <= levelMap.grid[gridX][gridZ].height) {
            projectiles.erase(projectiles.begin() + i);
            i--;
            continue;
          }
        } else if (p.type == 1) {
          // Mortar Parabolic Physics
          p.pos += p.velocity * deltaTime;
          p.velocity.y -= cfg.mortarGravity * deltaTime;

          bool hitGround = false;
          bool hitEnemy = false;

          // Enemy collision
          for (Enemy &e : enemies) {
            if (glm::distance(p.pos, e.currentPos +
                                         glm::vec3(0.0f, 0.4f, 0.0f)) < 0.6f) {
              hitEnemy = true;
              break;
            }
          }

          // Kolizja z terenem
          int gridX =
              std::clamp((int)std::round(p.pos.x), 0, levelMap.width - 1);
          int gridZ =
              std::clamp((int)std::round(p.pos.z), 0, levelMap.height - 1);
          if (p.pos.y <= levelMap.grid[gridX][gridZ].height) {
            hitGround = true;
          }

          if (hitGround || hitEnemy) {
            // Splash Damage
            for (Enemy &e : enemies) {
              float dist = glm::distance(p.pos, e.currentPos);
              if (dist <= cfg.towerMortarSplashRadius) {
                e.hp -= p.damage;
              }
            }
            spawnExplosion(p.pos, cfg.colorExplosionMortar,
                           cfg.explosionDurationMortar,
                           cfg.towerMortarSplashRadius, particles, flashes);
            projectiles.erase(projectiles.begin() + i);
            i--;
            continue;
          }
        }
      }

      // Enemy movement and death
      for (size_t i = 0; i < enemies.size(); i++) {
        Enemy &e = enemies[i];

        // Death check and reward
        if (e.hp <= 0.0f) {
          materials += e.reward;
          score += e.reward * 10;
          spawnExplosion(e.currentPos + glm::vec3(0.0f, 0.4f, 0.0f),
                         cfg.colorExplosionEnemy, cfg.explosionDurationEnemy,
                         1.0f, particles, flashes);
          enemies.erase(enemies.begin() + i);
          i--;
          continue;
        }

        e.progress += cfg.enemySpeed * deltaTime;

        if (e.progress >= 1.0f) {
          e.progress -= 1.0f;
          e.pathIndex++;
        }

        // Check if reached base
        if (e.pathIndex >= (int)levelMap.path.size() - 1) {
          enemies.erase(enemies.begin() + i);
          i--;

          // Enemy reached base
          playerHP--;
          if (playerHP <= 0) {
            playerHP = 0;
            gameOver = true;
          }

          continue;
        }

        // Path interpolation
        glm::vec2 p1 = glm::vec2((float)levelMap.path[e.pathIndex].x,
                                 (float)levelMap.path[e.pathIndex].y);
        glm::vec2 p2 = glm::vec2((float)levelMap.path[e.pathIndex + 1].x,
                                 (float)levelMap.path[e.pathIndex + 1].y);

        glm::vec2 current2D = glm::mix(p1, p2, e.progress);
        e.currentPos = glm::vec3(current2D.x, 0.6f, current2D.y);
      }

      // Particle physics
      for (size_t i = 0; i < particles.size(); i++) {
        particles[i].pos += particles[i].velocity * deltaTime;
        particles[i].velocity *= 0.95f;
        particles[i].lifeTime -= deltaTime;
        if (particles[i].lifeTime <= 0.0f) {
          particles.erase(particles.begin() + i);
          i--;
        }
      }
      for (size_t i = 0; i < flashes.size(); i++) {
        flashes[i].lifeTime -= deltaTime;
        if (flashes[i].lifeTime <= 0.0f) {
          flashes.erase(flashes.begin() + i);
          i--;
        }
      }

    } // end game logic

    glClearColor(0.02f, 0.05f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    basicShader.use();
    basicShader.setFloat("objectAlpha", 1.0f);

    glm::mat4 projection =
        glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 100.0f);
    basicShader.setMat4("projection", projection);
    glm::mat4 view = camera.GetViewMatrix();
    basicShader.setMat4("view", view);

    basicShader.setVec3("viewPos", camera.Position);

    // Directional light
    basicShader.setVec3("dirLight.direction", glm::vec3(-0.3f, -1.0f, -0.4f));
    basicShader.setVec3("dirLight.color", glm::vec3(0.35f, 0.35f, 0.45f));

    // Dynamic point lights from projectiles, lasers and explosions
    struct PointLightData {
      glm::vec3 pos;
      glm::vec3 color;
    };
    std::vector<PointLightData> allLights;
    for (const Projectile &p : projectiles) {
      allLights.push_back({p.pos, p.color});
    }
    for (const Tower &tow : placedTowers) {
      if (tow.type == 1 && tow.isFiringLaser) {
        float chargeRatio =
            std::clamp(tow.lockTime / cfg.towerLaserChargeTime, 0.0f, 1.0f);
        glm::vec3 baseColor = glm::vec3(0.0f, 1.0f, 1.0f);
        glm::vec3 maxColor = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::vec3 finalColor = glm::mix(baseColor, maxColor, chargeRatio);

        // Offset light slightly toward tower to avoid terrain clipping
        glm::vec3 towerEye(tow.x * 1.0f,
                           levelMap.grid[tow.x][tow.z].height + 1.2f,
                           tow.z * 1.0f);
        glm::vec3 dirToTower = glm::normalize(towerEye - tow.laserHitPos);
        glm::vec3 lightPos = tow.laserHitPos + dirToTower * 0.1f;

        allLights.push_back(
            {lightPos, finalColor * cfg.pointLightBrightnessMultiplier});
      }
    }
    for (const FlashLight &f : flashes) {
      float ratio = std::clamp(f.lifeTime / f.maxLifeTime, 0.0f, 1.0f);
      allLights.push_back({f.pos, f.color * ratio * 2.0f});
    }

    int numLights = std::min((int)allLights.size(), 32);
    basicShader.setInt("numPointLights", numLights);
    for (int i = 0; i < numLights; i++) {
      std::string prefix = "pointLights[" + std::to_string(i) + "].";
      basicShader.setVec3(prefix + "position", allLights[i].pos);
      basicShader.setVec3(prefix + "color", allLights[i].color);
      basicShader.setFloat(prefix + "constant", 1.0f);
      basicShader.setFloat(prefix + "linear", 0.14f);
      basicShader.setFloat(prefix + "quadratic", 0.07f);
    }

    glBindVertexArray(VAO);

    // Render map tiles
    for (int x = 0; x < levelMap.width; x++) {
      for (int z = 0; z < levelMap.height; z++) {
        Tile &t = levelMap.grid[x][z];

        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model,
                               glm::vec3(x * 1.0f, t.height / 2.0f, z * 1.0f));
        model = glm::scale(model, glm::vec3(0.95f, t.height, 0.95f));

        basicShader.setMat4("model", model);

        glm::vec3 objColor;
        if (t.type == TILE_PORTAL)
          objColor = cfg.colorPortal;
        else if (t.type == TILE_BASE)
          objColor = cfg.colorBase;
        else if (t.type == TILE_PATH)
          objColor = cfg.colorPath;
        else {
          float hRatio = std::clamp(t.height / 5.0f, 0.0f, 1.0f);
          objColor =
              glm::mix(cfg.colorTerrainLow, cfg.colorTerrainHigh, hRatio);
        }

        basicShader.setVec3("objectColor", objColor);

        glDrawArrays(GL_TRIANGLES, 0, 36);
      }
    }

    // Render enemies
    for (const Enemy &e : enemies) {
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, e.currentPos);
      model = glm::scale(model, glm::vec3(0.4f));
      basicShader.setMat4("model", model);

      basicShader.setVec3("objectColor", glm::vec3(1.0f, 0.5f, 0.0f));
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Render towers
    for (const Tower &tow : placedTowers) {
      Tile &t = levelMap.grid[tow.x][tow.z];
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model,
                             glm::vec3(tow.x * 1.0f, t.height, tow.z * 1.0f));
      model = glm::scale(model, glm::vec3(0.6f, 1.2f, 0.6f));
      basicShader.setMat4("model", model);

      glm::vec3 tColor = (tow.type == 0)   ? cfg.colorTowerBasic
                         : (tow.type == 1) ? cfg.colorTowerLaser
                                           : cfg.colorTowerMortar;
      basicShader.setVec3("objectColor", tColor);
      basicShader.setFloat("objectAlpha", 1.0f);

      glBindVertexArray(towerVAO[tow.type]);
      glDrawArrays(GL_TRIANGLES, 0, towerVertexCount[tow.type]);
    }

    // Render projectiles
    glBindVertexArray(VAO);
    for (const Projectile &p : projectiles) {
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, p.pos);
      model = glm::scale(model, glm::vec3(0.2f));
      basicShader.setMat4("model", model);

      basicShader.setVec3("objectColor", p.color * cfg.projectileBrightness);
      basicShader.setFloat("objectAlpha", 1.0f);

      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Render laser beams
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(towerVAO[2]);
    for (const Tower &tow : placedTowers) {
      if (tow.type == 1 && tow.isFiringLaser) {
        glm::vec3 towerEye(tow.x * 1.0f,
                           levelMap.grid[tow.x][tow.z].height + 1.2f,
                           tow.z * 1.0f);
        float dist = glm::distance(towerEye, tow.laserHitPos);

        if (dist > 0.01f) {
          float chargeRatio =
              std::clamp(tow.lockTime / cfg.towerLaserChargeTime, 0.0f, 1.0f);
          float thickness = 0.02f + 0.08f * chargeRatio;

          glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
          glm::vec3 fwd = glm::normalize(towerEye - tow.laserHitPos);
          if (std::abs(glm::dot(fwd, up)) > 0.999f) {
            up = glm::vec3(1.0f, 0.0f, 0.0f);
          }

          glm::mat4 model =
              glm::inverse(glm::lookAt(towerEye, tow.laserHitPos, up));
          model = glm::rotate(model, glm::radians(-90.0f),
                              glm::vec3(1.0f, 0.0f, 0.0f));
          model = glm::scale(model, glm::vec3(thickness, dist, thickness));

          basicShader.setMat4("model", model);

          glm::vec3 baseColor = glm::vec3(0.0f, 1.0f, 1.0f);
          glm::vec3 maxColor = glm::vec3(1.0f, 1.0f, 1.0f);
          glm::vec3 finalColor = glm::mix(baseColor, maxColor, chargeRatio);

          basicShader.setVec3("objectColor", finalColor * cfg.laserBrightness);
          basicShader.setFloat("objectAlpha", 0.7f + 0.3f * chargeRatio);

          glDrawArrays(GL_TRIANGLES, 0, towerVertexCount[2]);
        }
      }
    }
    glDisable(GL_BLEND);
    basicShader.setFloat("objectAlpha", 1.0f);

    // Render explosion particles
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glBindVertexArray(VAO);
    for (const Particle &pt : particles) {
      float ratio = std::clamp(pt.lifeTime / pt.maxLifeTime, 0.0f, 1.0f);

      // Zmniejszają się w miarę zanikania
      float currentScale = pt.initialScale * ratio;

      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, pt.pos);
      model = glm::rotate(model, pt.pos.x * 10.0f, glm::vec3(0.0f, 1.0f, 0.0f));
      model = glm::rotate(model, pt.pos.z * 10.0f, glm::vec3(1.0f, 0.0f, 0.0f));
      model = glm::scale(model, glm::vec3(currentScale));

      basicShader.setMat4("model", model);
      basicShader.setVec3("objectColor", pt.color * (1.0f + ratio));
      basicShader.setFloat("objectAlpha", ratio);

      glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    basicShader.setFloat("objectAlpha", 1.0f);

    // Render tower ghost during placement
    if (selectedTowerBuild != -1 && hoverValid) {
      Tile &t = levelMap.grid[hoverX][hoverZ];
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model,
                             glm::vec3(hoverX * 1.0f, t.height, hoverZ * 1.0f));
      model = glm::scale(model, glm::vec3(0.6f, 1.2f, 0.6f));
      basicShader.setMat4("model", model);

      glm::vec3 tColor = (selectedTowerBuild == 0)   ? cfg.colorTowerBasic
                         : (selectedTowerBuild == 1) ? cfg.colorTowerLaser
                                                     : cfg.colorTowerMortar;
      // Red tint if blocked
      if (t.type != TILE_EMPTY || t.hasTower) {
        tColor = glm::vec3(1.0f, 0.0f, 0.0f);
      }
      basicShader.setVec3("objectColor", tColor);
      basicShader.setFloat("objectAlpha", 0.5f);

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glBindVertexArray(towerVAO[selectedTowerBuild]);
      glDrawArrays(GL_TRIANGLES, 0, towerVertexCount[selectedTowerBuild]);

      glDisable(GL_BLEND);
      basicShader.setFloat("objectAlpha", 1.0f);
    }

    bool drawRange = false;
    float rX = 0, rZ = 0, rHeight = 0, rBaseRange = 0;

    if (selectedTowerBuild != -1 && hoverValid) {
      drawRange = true;
      rX = hoverX;
      rZ = hoverZ;
      rHeight = levelMap.grid[hoverX][hoverZ].height;
      rBaseRange = cfg.towerBasicRange;
      if (selectedTowerBuild == 1)
        rBaseRange = cfg.towerLaserRange;
      if (selectedTowerBuild == 2)
        rBaseRange = cfg.towerMortarRange;
    } else if (selectedPlacedTowerIndex != -1 &&
               selectedPlacedTowerIndex < (int)placedTowers.size()) {
      drawRange = true;
      Tower &tw = placedTowers[selectedPlacedTowerIndex];
      rX = tw.x;
      rZ = tw.z;
      rHeight = levelMap.grid[tw.x][tw.z].height;
      rBaseRange = cfg.towerBasicRange;
      if (tw.type == 1)
        rBaseRange = cfg.towerLaserRange;
      if (tw.type == 2)
        rBaseRange = cfg.towerMortarRange;
    }

    if (drawRange) {
      float actualRange =
          rBaseRange * (1.0f + rHeight * cfg.heightRangeMultiplier);
      int segments = 128;
      std::vector<float> rangePoints;
      rangePoints.reserve(segments * 3);

      float towerEyeY = rHeight + 1.2f;
      float targetY = 0.5f;

      for (int i = 0; i < segments; i++) {
        float angle = (2.0f * 3.14159265f * i) / segments;
        float dirX = cos(angle);
        float dirZ = sin(angle);

        float hitX = rX + dirX * actualRange;
        float hitZ = rZ + dirZ * actualRange;
        float hitY = rHeight + 0.1f;

        bool blocked = false;
        float step = 0.2f;
        for (float d = 0.0f; d <= actualRange; d += step) {
          float px = rX + dirX * d;
          float pz = rZ + dirZ * d;

          int gx = std::clamp((int)std::round(px), 0, levelMap.width - 1);
          int gz = std::clamp((int)std::round(pz), 0, levelMap.height - 1);

          float rayY = towerEyeY - ((towerEyeY - targetY) * (d / actualRange));

          // LoS terrain collision
          if (levelMap.grid[gx][gz].height > rayY) {
            hitX = px;
            hitZ = pz;
            hitY = levelMap.grid[gx][gz].height + 0.1f;
            blocked = true;
            break;
          }
        }

        // Snap ring to ground level
        if (!blocked) {
          int gx = std::clamp((int)std::round(hitX), 0, levelMap.width - 1);
          int gz = std::clamp((int)std::round(hitZ), 0, levelMap.height - 1);
          hitY = levelMap.grid[gx][gz].height + 0.1f;
        }

        rangePoints.push_back(hitX);
        rangePoints.push_back(hitY);
        rangePoints.push_back(hitZ);
      }

      glBindVertexArray(rangeVAO);
      glBindBuffer(GL_ARRAY_BUFFER, rangeVBO);
      glBufferSubData(GL_ARRAY_BUFFER, 0, rangePoints.size() * sizeof(float),
                      rangePoints.data());

      glm::mat4 idModel = glm::mat4(1.0f);
      basicShader.setMat4("model", idModel);
      basicShader.setVec3("objectColor",
                          glm::vec3(0.1f, 1.0f, 0.3f)); // Emissive Green Line

      glLineWidth(3.0f);
      glDrawArrays(GL_LINE_LOOP, 0, segments);
      glLineWidth(1.0f);
    }

    glBindVertexArray(VAO);

    // ImGui rendering
    renderUI(window, fbW, fbH, aspect, camera, enemies, placedTowers, levelMap);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glfwTerminate();
  return 0;
}

void processInput(GLFWwindow *window) {
  if (gamePaused)
    return;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    camera.ProcessKeyboard(ZOOM_IN, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    camera.ProcessKeyboard(ZOOM_OUT, deltaTime);
}

void scroll_callback(GLFWwindow * /*window*/, double /*xoffset*/,
                     double yoffset) {
  if (ImGui::GetIO().WantCaptureMouse)
    return;
  camera.ProcessMouseScroll((float)yoffset);
}

void framebuffer_size_callback(GLFWwindow * /*window*/, int width, int height) {
  glViewport(0, 0, width, height);
}
