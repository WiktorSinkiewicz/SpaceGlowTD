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
#include <utility>
#include <vector>

#include "Camera.h"
#include "Map.h"
#include "Shader.h"

struct Enemy {
  int id;
  int pathIndex;
  float progress;
  glm::vec3 currentPos;
  float hp;
  float maxHp;
  int reward;
};

struct Tower {
  int x;
  int z;
  int type; // 0: Basic, 1: Laser, 2: Mortar
  float cooldownTimer;
  
  // Laser State
  int lockedTargetId = -1;
  float lockTime = 0.0f;
  glm::vec3 laserHitPos = glm::vec3(0.0f);
  bool isFiringLaser = false;
  
  // Upgrade State
  int upgradeLevel = 0;
};

struct Projectile {
  int type = 0; // 0: Basic (Homing), 1: Mortar (Parabolic)
  glm::vec3 pos;
  glm::vec3 velocity; // Używane głównie przez Moździerz
  int targetId;
  float damage;
  glm::vec3 color;
};

struct Particle {
  glm::vec3 pos;
  glm::vec3 velocity;
  float lifeTime;
  float maxLifeTime;
  glm::vec3 color;
  float initialScale;
};

struct FlashLight {
  glm::vec3 pos;
  glm::vec3 color;
  float lifeTime;
  float maxLifeTime;
};

void spawnExplosion(const glm::vec3& pos, const glm::vec3& color, float maxLife, float baseScale, std::vector<Particle>& particles, std::vector<FlashLight>& flashes) {
    FlashLight flash;
    flash.pos = pos;
    flash.color = color;
    flash.lifeTime = maxLife * 0.5f; // Krótki błysk
    flash.maxLifeTime = maxLife * 0.5f;
    flashes.push_back(flash);

    int numParticles = 20 + rand() % 15;
    for (int j = 0; j < numParticles; j++) {
        Particle pt;
        pt.pos = pos;
        
        // Random direction w sferze 3D
        float r1 = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float r2 = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float r3 = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        
        // Dodaj lekkie wychylenie do góry, żeby eksplozja wywalała w górę
        glm::vec3 dir = glm::normalize(glm::vec3(r1, r2 + 0.5f, r3));
        
        float speed = 3.0f + ((float)rand() / RAND_MAX) * 5.0f;
        pt.velocity = dir * speed;
        
        pt.maxLifeTime = maxLife * (0.5f + ((float)rand() / RAND_MAX) * 0.5f);
        pt.lifeTime = pt.maxLifeTime;
        
        // Lekka wariacja koloru (np. trochę żółci dla urozmaicenia ognia)
        pt.color = color;
        if (rand() % 3 == 0) {
            pt.color = glm::mix(color, glm::vec3(1.0f, 1.0f, 1.0f), 0.5f);
        }
        
        pt.initialScale = baseScale * (0.2f + ((float)rand() / RAND_MAX) * 0.3f);
        particles.push_back(pt);
    }
}

static int nextEnemyId = 1;

std::vector<float> generatePrismVertices(int sides) {
  std::vector<float> vertices;
  const float PI = 3.14159265f;
  float angleStep = 2.0f * PI / sides;

  // Rotate square by 45 degrees to align with grid, and scale so half-width is
  // 0.5
  float angleOffset = (sides == 4) ? (PI / 4.0f) : 0.0f;
  float radius = (sides == 4) ? (0.5f * sqrt(2.0f)) : 0.5f;

  for (int i = 0; i < sides; ++i) {
    float a1 = i * angleStep + angleOffset;
    float a2 = (i + 1) * angleStep + angleOffset;

    float x1 = radius * cos(a1);
    float z1 = radius * sin(a1);
    float x2 = radius * cos(a2);
    float z2 = radius * sin(a2);

    // Calculate normal for this face
    glm::vec3 normal =
        glm::normalize(glm::vec3((x1 + x2) / 2.0f, 0.0f, (z1 + z2) / 2.0f));

    // Side face (2 triangles)
    vertices.insert(vertices.end(),
                    {x1, 0.0f, z1, normal.x, normal.y, normal.z});
    vertices.insert(vertices.end(),
                    {x1, 1.0f, z1, normal.x, normal.y, normal.z});
    vertices.insert(vertices.end(),
                    {x2, 0.0f, z2, normal.x, normal.y, normal.z});

    vertices.insert(vertices.end(),
                    {x2, 0.0f, z2, normal.x, normal.y, normal.z});
    vertices.insert(vertices.end(),
                    {x1, 1.0f, z1, normal.x, normal.y, normal.z});
    vertices.insert(vertices.end(),
                    {x2, 1.0f, z2, normal.x, normal.y, normal.z});

    // Top face
    vertices.insert(vertices.end(), {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {x2, 1.0f, z2, 0.0f, 1.0f, 0.0f});
    vertices.insert(vertices.end(), {x1, 1.0f, z1, 0.0f, 1.0f, 0.0f});

    // Bottom face
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f});
    vertices.insert(vertices.end(), {x1, 0.0f, z1, 0.0f, -1.0f, 0.0f});
    vertices.insert(vertices.end(), {x2, 0.0f, z2, 0.0f, -1.0f, 0.0f});
  }
  return vertices;
}

bool intersectRayAABB(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::vec3 boxMin,
                      glm::vec3 boxMax, float &tOut) {
  float tmin = (boxMin.x - rayOrigin.x) / rayDir.x;
  float tmax = (boxMax.x - rayOrigin.x) / rayDir.x;
  if (tmin > tmax)
    std::swap(tmin, tmax);

  float tymin = (boxMin.y - rayOrigin.y) / rayDir.y;
  float tymax = (boxMax.y - rayOrigin.y) / rayDir.y;
  if (tymin > tymax)
    std::swap(tymin, tymax);

  if ((tmin > tymax) || (tymin > tmax))
    return false;

  if (tymin > tmin)
    tmin = tymin;
  if (tymax < tmax)
    tmax = tymax;

  float tzmin = (boxMin.z - rayOrigin.z) / rayDir.z;
  float tzmax = (boxMax.z - rayOrigin.z) / rayDir.z;
  if (tzmin > tzmax)
    std::swap(tzmin, tzmax);

  if ((tmin > tzmax) || (tzmin > tmax))
    return false;

  if (tzmin > tmin)
    tmin = tzmin;

  if (tmin < 0)
    return false;
  tOut = tmin;
  return true;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

namespace {

// Bazowy rozmiar interfejsu: powiększa się wraz z krótszym bokiem okna
// (referencja ~640 px)
constexpr float kUiRefShortSide = 640.f;

// Rysuje ikonę wieży w środku danego promienia: 0 = prostokąt, 1 = trójkąt, 2 =
// sześciokąt
void DrawTowerShapeIcon(ImDrawList *dl, ImVec2 center, float radius,
                        int towerIndex, float lineThick) {
  const ImU32 fill = IM_COL32(100, 190, 255, 255);
  const ImU32 stroke = IM_COL32(255, 255, 255, 230);

  switch (towerIndex) {
  case 0: {
    ImVec2 a(center.x - radius * 0.85f, center.y - radius * 0.85f);
    ImVec2 b(center.x + radius * 0.85f, center.y + radius * 0.85f);
    dl->AddRectFilled(a, b, fill, 3.f);
    dl->AddRect(a, b, stroke, 3.f, 0, lineThick);
    break;
  }
  case 1: {
    ImVec2 p0(center.x, center.y - radius);
    ImVec2 p1(center.x - radius * 0.92f, center.y + radius * 0.55f);
    ImVec2 p2(center.x + radius * 0.92f, center.y + radius * 0.55f);
    dl->AddTriangleFilled(p0, p1, p2, fill);
    ImVec2 tri[3] = {p0, p1, p2};
    dl->AddPolyline(tri, 3, stroke, ImDrawFlags_Closed, lineThick);
    break;
  }
  case 2: {
    constexpr float kPi = 3.14159265f;
    ImVec2 hex[6];
    for (int i = 0; i < 6; ++i) {
      float ang = -kPi * 0.5f + (kPi / 3.f) * static_cast<float>(i);
      hex[i] = ImVec2(center.x + std::cos(ang) * radius,
                      center.y + std::sin(ang) * radius);
    }
    dl->AddConvexPolyFilled(hex, 6, fill);
    dl->AddPolyline(hex, 6, stroke, ImDrawFlags_Closed, lineThick);
    break;
  }
  default:
    break;
  }
}

} // namespace

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Odsunięty mocno widok RTS, wyśrodkowany nad mapą 20x20!
// (środek mapy to X=10, Z=10). Y=15 to wysokość latania nad ziemią.
Camera camera(glm::vec3(10.0f, 15.0f, 22.0f));

float deltaTime = 0.0f;
float lastFrame = 0.0f;

#include "GameConfig.h"

GameConfig cfg;

// Stan gry
int playerHP = cfg.initialPlayerHP;
int materials = cfg.initialMaterials;
int score = 0;
bool gameOver = false;
bool gamePaused = false;

// System fal (Waves)
int currentWave = 1;
int enemiesSpawnedInWave = 0;
float waveTimer = cfg.timeBetweenWaves;
bool waveInProgress = false;

// -1 = brak wyboru, 0 = zwykła wieża, 1 = laser, 2 = moździerz (do przyszłego
// stawiania na mapie)
int selectedTowerBuild = -1;
int selectedPlacedTowerIndex = -1; // -1 = brak zaznaczonej wieży

static ImGuiStyle g_imguiStyleBaseline;
static float s_imguiLastScale = -1.f;

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(
      SCR_WIDTH, SCR_HEIGHT, "SpaceGlowTD - Voxel Terrain", NULL, NULL);
  if (window == NULL) {
    std::cerr << "Blad przy ladowaniu okna GLFW!" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync to prevent stuttering and 100% GPU usage
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetScrollCallback(window, scroll_callback);

  // Kursor pozostaje włączony, gdyż jest on RTS/TD grą, w KROKU 4 dodamy
  // Raycasting.
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Blad przy inicjalizacji GLAD!" << std::endl;
    return -1;
  }

  glEnable(GL_DEPTH_TEST);

  // Odpalenie culling (nie renderuj ścian z tyłu kostki względem kamery, da nam
  // to FPS w 400 klockach bez problemu)
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  // Inicjalizacja Dear ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  ImGui::StyleColorsDark();
  g_imguiStyleBaseline = ImGui::GetStyle();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  Shader basicShader("shaders/basic.vert", "shaders/basic.frag");

  // Pozycja (XYZ) + Normalna (XYZ) = 6 floatów na wierzchołek, 36 wierzchołków
  // = 216 floatów
  float vertices[] = {
      // Tylna sciana - normalna (0, 0, -1)
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.5f, 0.5f, -0.5f, 0.0f, 0.0f,
      -1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.5f, 0.5f, -0.5f, 0.0f,
      0.0f, -1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, -0.5f, 0.5f, -0.5f,
      0.0f, 0.0f, -1.0f,
      // Przednia sciana - normalna (0, 0, 1)
      -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
      0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
      -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
      // Lewa sciana - normalna (-1, 0, 0)
      -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, -1.0f, 0.0f,
      0.0f, -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, -0.5f, -0.5f, -0.5f, -1.0f,
      0.0f, 0.0f, -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, -0.5f, 0.5f, 0.5f,
      -1.0f, 0.0f, 0.0f,
      // Prawa sciana - normalna (1, 0, 0)
      0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
      0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
      0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
      // Dolna sciana - normalna (0, -1, 0)
      -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.5f, -0.5f, -0.5f, 0.0f, -1.0f,
      0.0f, 0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.5f, -0.5f, 0.5f, 0.0f,
      -1.0f, 0.0f, -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, -0.5f, -0.5f, -0.5f,
      0.0f, -1.0f, 0.0f,
      // Gorna sciana - normalna (0, 1, 0)
      -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
      0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
      -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, -0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
      0.0f};

  unsigned int VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Atrybut 0: Pozycja (3 floaty, stride 6 = przeskok do następnego
  // wierzchołka)
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  // Atrybut 1: Normalna (3 floaty, offset 3 = za pozycją)
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  GLuint towerVAO[3], towerVBO[3];
  int towerVertexCount[3];
  int sides[3] = {4, 3, 6}; // Square, Triangle, Hexagon
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
  // Max 128 points -> 128 * 3 floats
  glBufferData(GL_ARRAY_BUFFER, 128 * 3 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  std::vector<Tower> placedTowers;

  // KROK 2 - Generowanie struktury gry
  Map levelMap(20, 20);
  levelMap.generateTerrain();

  // Generowanie Bazy, Portalu i mutowanie proceduralnej ewolucyjnej Ścieżki z
  // limitem w okolicy 40-52 klatek do celów Tabu Search
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
          placedTowers.push_back({hoverX, hoverZ, selectedTowerBuild, 0.0f, -1, 0.0f, glm::vec3(0.0f), false, 0});
          selectedTowerBuild = -1; // Deselect after build
        }
      } else if (selectedTowerBuild == -1 && t.hasTower) {
        // Znajdź wieżę i zaznacz ją
        selectedPlacedTowerIndex = -1;
        for (size_t i = 0; i < placedTowers.size(); i++) {
          if (placedTowers[i].x == hoverX && placedTowers[i].z == hoverZ) {
            selectedPlacedTowerIndex = (int)i;
            break;
          }
        }
      } else if (selectedTowerBuild == -1 && !t.hasTower) {
        // Kliknięcie w puste pole odznacza wieżę
        selectedPlacedTowerIndex = -1;
      }
    }
    mouseLeftPrev = mouseLeftDown;

    static bool mouseRightPrev = false;
    bool mouseRightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (mouseRightDown && !mouseRightPrev) {
        selectedTowerBuild = -1;
        selectedPlacedTowerIndex = -1;
    }
    mouseRightPrev = mouseRightDown;

    // Logika gry zatrzymuje się po Game Over lub w pauzie
    if (!gameOver && !gamePaused) {

      // System Spawnowania i Fal
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
            e.currentPos = glm::vec3(levelMap.path[0].x, 0.4f,
                                     levelMap.path[0].y); // Y jako Z

            // Skalowanie HP wroga z każdą falą
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
          // Koniec obecnej fali
          waveInProgress = false;
          currentWave++;
          enemiesSpawnedInWave = 0;
          waveTimer = cfg.timeBetweenWaves;
        }
      } else {
        // Oczekiwanie na następną falę
        waveTimer -= deltaTime;
        if (waveTimer <= 0.0f) {
          waveInProgress = true;
          spawnTimer = cfg.spawnRate; // Wywołaj spawn pierwszego wroga od razu
        }
      }

      // --------------------------------
      // Logika Wież (Strzelanie)
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
            // Pocisk wylatuje ze szczytu wieży (Y bazy + 1.2f wysokość wieży)
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
              p.damage = cfg.towerBasicDamage * (1.0f + tow.upgradeLevel * 1.0f);
              // Świecący, jasny neonowo-błękitny kolor pocisku
              p.color = glm::vec3(0.1f, 1.0f, 0.9f);
              projectiles.push_back(p);
              tow.cooldownTimer = cfg.towerBasicCooldown;
            }
          }
        } else if (tow.type == 1) { // Laser Tower
          float actualRange = cfg.towerLaserRange * (1.0f + levelMap.grid[tow.x][tow.z].height * cfg.heightRangeMultiplier);
          glm::vec3 towerPos(tow.x * 1.0f, levelMap.grid[tow.x][tow.z].height + 1.2f, tow.z * 1.0f);
          
          tow.isFiringLaser = false;
          
          Enemy* target = nullptr;
          
          if (tow.lockedTargetId != -1) {
              for (Enemy &e : enemies) {
                  if (e.id == tow.lockedTargetId) {
                      target = &e;
                      break;
                  }
              }
          }
          
          if (!target || glm::distance(towerPos, target->currentPos) > actualRange) {
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
              glm::vec3 targetPos = target->currentPos + glm::vec3(0.0f, 0.4f, 0.0f);
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
                  
                  float damageScale = 1.0f + (tow.lockTime / cfg.towerLaserChargeTime) * (cfg.towerLaserDamageMaxMultiplier - 1.0f);
                  damageScale = std::min(damageScale, cfg.towerLaserDamageMaxMultiplier);
                  
                  target->hp -= cfg.towerLaserDamageBase * damageScale * (1.0f + tow.upgradeLevel * 1.0f) * deltaTime;
              }
          }
        } else if (tow.type == 2) { // Mortar Tower
          float actualRange = cfg.towerMortarRange * (1.0f + levelMap.grid[tow.x][tow.z].height * cfg.heightRangeMultiplier);
          glm::vec3 towerPos(tow.x * 1.0f, levelMap.grid[tow.x][tow.z].height + 1.2f, tow.z * 1.0f);
          
          if (tow.cooldownTimer <= 0.0f) {
              float closestDistSq = actualRange * actualRange;
              Enemy* target = nullptr;
              
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
                  
                  // Przewidywanie ruchu wroga (Lead Target)
                  // Szacunkowy czas lotu na podstawie obecnego dystansu:
                  float estimatedFlightTime = initialDistXZ / cfg.mortarHorizontalSpeed;
                  
                  // Przewidujemy gdzie wróg będzie na ścieżce za czas estimatedFlightTime
                  float futureProgress = target->progress + cfg.enemySpeed * estimatedFlightTime;
                  int futurePathIndex = target->pathIndex;
                  while (futureProgress >= 1.0f && futurePathIndex < (int)levelMap.path.size() - 2) {
                      futureProgress -= 1.0f;
                      futurePathIndex++;
                  }
                  
                  // Znajdź pozycję na zinterpolowanej krzywej
                  glm::vec2 p1 = glm::vec2((float)levelMap.path[futurePathIndex].x, (float)levelMap.path[futurePathIndex].y);
                  glm::vec2 p2 = glm::vec2((float)levelMap.path[futurePathIndex + 1].x, (float)levelMap.path[futurePathIndex + 1].y);
                  glm::vec2 predictedXZ = glm::mix(p1, p2, futureProgress);
                  glm::vec3 predictedPos = glm::vec3(predictedXZ.x, target->currentPos.y, predictedXZ.y); // Y zostaje bez zmian (na ziemi)
                  
                  float finalDistXZ = glm::distance(startXZ, predictedXZ);
                  
                  if (finalDistXZ > 0.1f) {
                      // Obliczamy ostateczny wektor prędkości aby uderzyć w predictedPos
                      float flightTime = finalDistXZ / cfg.mortarHorizontalSpeed;
                      
                      float heightDiff = predictedPos.y - towerPos.y;
                      float v0y = (heightDiff + 0.5f * cfg.mortarGravity * flightTime * flightTime) / flightTime;
                      
                      glm::vec2 dirXZ = glm::normalize(predictedXZ - startXZ) * cfg.mortarHorizontalSpeed;
                      
                      Projectile p;
                      p.type = 1;
                      p.pos = towerPos;
                      p.velocity = glm::vec3(dirXZ.x, v0y, dirXZ.y);
                      p.targetId = -1; // Moździerz nie śledzi, strzela "na ślepo" w punkt
                      p.damage = cfg.towerMortarDamage * (1.0f + tow.upgradeLevel * 1.0f);
                      p.color = glm::vec3(1.0f, 0.5f, 0.0f);
                      projectiles.push_back(p);
                      
                      tow.cooldownTimer = cfg.towerMortarCooldown;
                  }
              }
          }
        }
      }

      // --------------------------------
      // Logika Pocisków (Homing i Fizyka)
      // --------------------------------
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
              // Namierzaj na środek masy wroga, nie na jego 'stopy'
              glm::vec3 aimPos = target->currentPos + glm::vec3(0.0f, 0.4f, 0.0f);
              glm::vec3 dir = glm::normalize(aimPos - p.pos);
              p.pos += dir * cfg.projectileSpeed * deltaTime;

              // Kolizja sferyczna (trafienie we wroga)
              if (glm::distance(p.pos, aimPos) < 0.5f) {
                target->hp -= p.damage;
                projectiles.erase(projectiles.begin() + i);
                i--;
                continue;
              }
            } else {
              // Jeśli cel zginął zanim pocisk doleciał, opada zgodnie z grawitacją
              p.pos.y -= cfg.projectileSpeed * 0.5f * deltaTime;
            }

            // Kolizja z terenem pod spodem
            int gridX = std::clamp((int)std::round(p.pos.x), 0, levelMap.width - 1);
            int gridZ = std::clamp((int)std::round(p.pos.z), 0, levelMap.height - 1);
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
            
            // Kolizja z wrogiem
            for (Enemy &e : enemies) {
                if (glm::distance(p.pos, e.currentPos + glm::vec3(0.0f, 0.4f, 0.0f)) < 0.6f) {
                    hitEnemy = true;
                    break;
                }
            }
            
            // Kolizja z terenem
            int gridX = std::clamp((int)std::round(p.pos.x), 0, levelMap.width - 1);
            int gridZ = std::clamp((int)std::round(p.pos.z), 0, levelMap.height - 1);
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
                spawnExplosion(p.pos, cfg.colorExplosionMortar, cfg.explosionDurationMortar, cfg.towerMortarSplashRadius, particles, flashes);
                projectiles.erase(projectiles.begin() + i);
                i--;
                continue;
            }
        }
      }

      // Płynna Fizyka i Logika Trasy (UFO)
      for (size_t i = 0; i < enemies.size(); i++) {
        Enemy &e = enemies[i];

        // Sprawdzanie smierci i dodawanie nagrody (materials / gold)
        if (e.hp <= 0.0f) {
          materials += e.reward;
          score += e.reward * 10;
          spawnExplosion(e.currentPos + glm::vec3(0.0f, 0.4f, 0.0f), cfg.colorExplosionEnemy, cfg.explosionDurationEnemy, 1.0f, particles, flashes);
          enemies.erase(enemies.begin() + i);
          i--;
          continue;
        }

        e.progress += cfg.enemySpeed * deltaTime;

        if (e.progress >= 1.0f) {
          e.progress -= 1.0f;
          e.pathIndex++;
        }

        // Check Base collision
        if (e.pathIndex >= (int)levelMap.path.size() - 1) {
          enemies.erase(enemies.begin() + i);
          i--;

          // Wróg dotarł do bazy - utrata HP
          playerHP--;
          if (playerHP <= 0) {
            playerHP = 0;
            gameOver = true;
          }

          continue;
        }

        // Liniowa Interpolacja Mix w przestrzeni kafelków 3D
        glm::vec2 p1 = glm::vec2((float)levelMap.path[e.pathIndex].x,
                                 (float)levelMap.path[e.pathIndex].y);
        glm::vec2 p2 = glm::vec2((float)levelMap.path[e.pathIndex + 1].x,
                                 (float)levelMap.path[e.pathIndex + 1].y);

        glm::vec2 current2D = glm::mix(p1, p2, e.progress);
        // Y w grze rygujemy na płaskie 0.6f by najeżdżały na kostki
        e.currentPos = glm::vec3(current2D.x, 0.6f, current2D.y);
      }

      // Logika Particle System (Eksplozje)
      for (size_t i = 0; i < particles.size(); i++) {
        particles[i].pos += particles[i].velocity * deltaTime;
        particles[i].velocity *= 0.95f; // opór powietrza (drag)
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

    } // koniec if (!gameOver)

    glClearColor(0.02f, 0.05f, 0.10f,
                 1.0f); // Bardzo mroczne, głębokie kosmiczne tło
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    basicShader.use();
    basicShader.setFloat("objectAlpha", 1.0f);

    glm::mat4 projection =
        glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 100.0f);
    basicShader.setMat4("projection", projection);
    glm::mat4 view = camera.GetViewMatrix();
    basicShader.setMat4("view", view);

    // Oświetlenie: pozycja kamery do obliczeń refleksji lustrzanej
    basicShader.setVec3("viewPos", camera.Position);

    // Światło kierunkowe (kosmiczne słońce) - ciemniejsze tło by pociski ładniej świeciły
    basicShader.setVec3("dirLight.direction", glm::vec3(-0.3f, -1.0f, -0.4f));
    basicShader.setVec3("dirLight.color", glm::vec3(0.35f, 0.35f, 0.45f));

    // Dynamiczne światła punktowe od pocisków i trafień lasera!
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
            float chargeRatio = std::clamp(tow.lockTime / cfg.towerLaserChargeTime, 0.0f, 1.0f);
            glm::vec3 baseColor = glm::vec3(0.0f, 1.0f, 1.0f);
            glm::vec3 maxColor = glm::vec3(1.0f, 1.0f, 1.0f);
            glm::vec3 finalColor = glm::mix(baseColor, maxColor, chargeRatio);
            
            // Wycofujemy światło odrobinę w stronę wieży, by nie utknęło w geometrii,
            // ale polegamy głównie na nowym shaderze (burnGlow) do oświetlenia powierzchni!
            glm::vec3 towerEye(tow.x * 1.0f, levelMap.grid[tow.x][tow.z].height + 1.2f, tow.z * 1.0f);
            glm::vec3 dirToTower = glm::normalize(towerEye - tow.laserHitPos);
            glm::vec3 lightPos = tow.laserHitPos + dirToTower * 0.1f;
            
            allLights.push_back({lightPos, finalColor * cfg.pointLightBrightnessMultiplier});
        }
    }
    for (const FlashLight &f : flashes) {
        float ratio = std::clamp(f.lifeTime / f.maxLifeTime, 0.0f, 1.0f);
        // Błysk jest najsilniejszy na początku
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

    // Renderowanie 400 Kafelków Mapy
    for (int x = 0; x < levelMap.width; x++) {
      for (int z = 0; z < levelMap.height; z++) {
        Tile &t = levelMap.grid[x][z];

        glm::mat4 model = glm::mat4(1.0f);

        // Przestawiamy każdy sześcian by spoczywał na dole X i Z. Ujemny punkt
        // obrotu Y ustawi go równo z Ziemią mimo skalowania.
        model = glm::translate(model,
                               glm::vec3(x * 1.0f, t.height / 2.0f, z * 1.0f));
        // Skalowanie wysokości (by udawało wyrastające voxelowe skały).
        // Odrobina luki by widzieć Grid (skaluje na np. 0.95 w boki)
        model = glm::scale(model, glm::vec3(0.95f, t.height, 0.95f));

        basicShader.setMat4("model", model);

        // Zarządzanie barwami pod mapę
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

    // Renderowanie Systemu Najeźdzców UFO po narysowaniu mapy
    for (const Enemy &e : enemies) {
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, e.currentPos);
      model = glm::scale(
          model,
          glm::vec3(0.4f, 0.4f, 0.4f)); // Mniejsze sześciany uciekają z Bazy
      basicShader.setMat4("model", model);

      // Pomaracznowy kolor wroga UFO
      basicShader.setVec3("objectColor", glm::vec3(1.0f, 0.5f, 0.0f));
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Renderowanie zbudowanych wież
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

    // Renderowanie aktywnych pocisków
    glBindVertexArray(VAO); // Przełączamy z powrotem na model Box'a
    for (const Projectile &p : projectiles) {
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, p.pos);
      model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f)); // Mała kostka
      basicShader.setMat4("model", model);

      // Emissive (niech ich kolor będzie mocny)
      basicShader.setVec3("objectColor", p.color * cfg.projectileBrightness); // "Wypalony" kolor
      basicShader.setFloat("objectAlpha", 1.0f);

      glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Renderowanie promieni laserowych
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(towerVAO[2]); // Używamy heksagonu jako cylindra promienia
    for (const Tower &tow : placedTowers) {
      if (tow.type == 1 && tow.isFiringLaser) {
        glm::vec3 towerEye(tow.x * 1.0f, levelMap.grid[tow.x][tow.z].height + 1.2f, tow.z * 1.0f);
        float dist = glm::distance(towerEye, tow.laserHitPos);
        
        if (dist > 0.01f) {
            float chargeRatio = std::clamp(tow.lockTime / cfg.towerLaserChargeTime, 0.0f, 1.0f);
            float thickness = 0.02f + 0.08f * chargeRatio; // Cieńszy laser
            
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 fwd = glm::normalize(towerEye - tow.laserHitPos);
            if (std::abs(glm::dot(fwd, up)) > 0.999f) {
                up = glm::vec3(1.0f, 0.0f, 0.0f);
            }
            
            glm::mat4 model = glm::inverse(glm::lookAt(towerEye, tow.laserHitPos, up));
            model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(thickness, dist, thickness));
            
            basicShader.setMat4("model", model);
            
            glm::vec3 baseColor = glm::vec3(0.0f, 1.0f, 1.0f); // Cyjan
            glm::vec3 maxColor = glm::vec3(1.0f, 1.0f, 1.0f); // Biały
            glm::vec3 finalColor = glm::mix(baseColor, maxColor, chargeRatio);
            
            basicShader.setVec3("objectColor", finalColor * cfg.laserBrightness);
            basicShader.setFloat("objectAlpha", 0.7f + 0.3f * chargeRatio);
            
            glDrawArrays(GL_TRIANGLES, 0, towerVertexCount[2]);

        }
      }
    }
    glDisable(GL_BLEND);
    basicShader.setFloat("objectAlpha", 1.0f);

    // Renderowanie Eksplozji (Particle System)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending
    glDepthMask(GL_FALSE); // Eksplozje nie piszą do z-buffera, przenikają
    glBindVertexArray(VAO); // Używamy Box'a (małe sześciany)
    for (const Particle &pt : particles) {
        float ratio = std::clamp(pt.lifeTime / pt.maxLifeTime, 0.0f, 1.0f);
        
        // Zmniejszają się w miarę zanikania
        float currentScale = pt.initialScale * ratio;
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pt.pos);
        // Opcjonalnie mały obrót oparty na pozycji (by nie wszystkie były proste)
        model = glm::rotate(model, pt.pos.x * 10.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, pt.pos.z * 10.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(currentScale));
        
        basicShader.setMat4("model", model);
        // Bardzo jasne na początku, potem wracają do bazowego koloru
        basicShader.setVec3("objectColor", pt.color * (1.0f + ratio));
        basicShader.setFloat("objectAlpha", ratio);
        
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    basicShader.setFloat("objectAlpha", 1.0f);

    // Renderowanie "Ducha" wieży podczas budowania
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
      // Zmień kolor na czerwony jeśli zablokowane
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

      // --- Zasięg wieży (Line of Sight i deformacja) ---
    }
    
    bool drawRange = false;
    float rX = 0, rZ = 0, rHeight = 0, rBaseRange = 0;
    
    if (selectedTowerBuild != -1 && hoverValid) {
        drawRange = true;
        rX = hoverX;
        rZ = hoverZ;
        rHeight = levelMap.grid[hoverX][hoverZ].height;
        rBaseRange = cfg.towerBasicRange;
        if (selectedTowerBuild == 1) rBaseRange = cfg.towerLaserRange;
        if (selectedTowerBuild == 2) rBaseRange = cfg.towerMortarRange;
    } else if (selectedPlacedTowerIndex != -1 && selectedPlacedTowerIndex < (int)placedTowers.size()) {
        drawRange = true;
        Tower& tw = placedTowers[selectedPlacedTowerIndex];
        rX = tw.x;
        rZ = tw.z;
        rHeight = levelMap.grid[tw.x][tw.z].height;
        rBaseRange = cfg.towerBasicRange;
        if (tw.type == 1) rBaseRange = cfg.towerLaserRange;
        if (tw.type == 2) rBaseRange = cfg.towerMortarRange;
    }

    if (drawRange) {
      float actualRange = rBaseRange * (1.0f + rHeight * cfg.heightRangeMultiplier);
      int segments = 128;
      std::vector<float> rangePoints;
      rangePoints.reserve(segments * 3);

      float towerEyeY = rHeight + 1.2f; // Oczy wieży
      float targetY = 0.5f; // Celuje w środek wroga

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

          // Kolizja LoS z terenem
          if (levelMap.grid[gx][gz].height > rayY) {
            hitX = px;
            hitZ = pz;
            hitY = levelMap.grid[gx][gz].height + 0.1f;
            blocked = true;
            break;
          }
        }

        // Zjazd w dół by okrąg leżał na ziemi
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

    glBindVertexArray(VAO); // Powrót na standardowy box

    // ========================
    // Dear ImGui - Renderowanie interfejsu
    // ========================
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    // Skalowanie całego UI względem rozmiaru okna (krótszy bok ~640 px =
    // skala 1.0)
    const float uiScale = std::clamp(
        std::min((float)fbW, (float)fbH) / kUiRefShortSide, 0.85f, 3.0f);

    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = uiScale;

    ImGuiStyle &style = ImGui::GetStyle();
    if (s_imguiLastScale < 0.f) {
      style = g_imguiStyleBaseline;
      style.ScaleAllSizes(uiScale);
      s_imguiLastScale = uiScale;
    } else if (std::fabs(uiScale - s_imguiLastScale) > 0.001f) {
      const float ratio = uiScale / s_imguiLastScale;
      style.ScaleAllSizes(ratio);
      s_imguiLastScale = uiScale;
    }

    ImGui::NewFrame();

    const float towerPanelW =
        std::clamp(148.f * uiScale, 120.f, 0.22f * (float)fbW);
    const float hudTopH = 56.f * uiScale;
    const float towerBtnH = 68.f * uiScale;
    const float iconLineThick = std::max(1.5f, 1.35f * uiScale);
    const float borderSel = std::max(2.f, 2.25f * uiScale);
    const float borderHover = std::max(1.25f, 1.5f * uiScale);
    const float borderNorm = std::max(1.f, 1.2f * uiScale);

    // Paski zdrowia wrogów (Renderowane w ImGui nad modelami 3D)
    ImDrawList *bgDrawList = ImGui::GetBackgroundDrawList();
    glm::mat4 viewM = camera.GetViewMatrix();
    glm::mat4 projM =
        glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 100.0f);

    for (const Enemy &e : enemies) {
      if (e.hp < e.maxHp &&
          e.hp > 0.0f) { // Pokaż pasek zdrowia tylko jeśli otrzymał obrażenia
        glm::vec4 clip =
            projM * viewM *
            glm::vec4(e.currentPos + glm::vec3(0.0f, 0.8f, 0.0f), 1.0f);
        if (clip.w > 0.1f) {
          glm::vec3 ndc = glm::vec3(clip) / clip.w;
          float screenX = (ndc.x + 1.0f) * 0.5f * fbW;
          float screenY = (1.0f - ndc.y) * 0.5f * fbH;

          float hpPercent = std::max(0.0f, e.hp / e.maxHp);
          float barW = 30.0f * uiScale;
          float barH = 4.0f * uiScale;

          ImVec2 pMin(screenX - barW * 0.5f, screenY);
          ImVec2 pMax(screenX + barW * 0.5f, screenY + barH);

          bgDrawList->AddRectFilled(pMin, pMax, IM_COL32(200, 20, 20, 255));
          bgDrawList->AddRectFilled(pMin,
                                    ImVec2(pMin.x + barW * hpPercent, pMax.y),
                                    IM_COL32(40, 200, 40, 255));
          bgDrawList->AddRect(pMin, pMax, IM_COL32(0, 0, 0, 255));
        }
      }
    }

    // Górny pasek HUD — szerokość bez prawego panelu wież
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)fbW - towerPanelW, hudTopH));
    ImGui::Begin("HUD", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBackground);

    // HP - czerwony gdy niskie, zielony gdy pełne
    float hpRatio = (float)playerHP / 10.0f;
    ImVec4 hpColor = ImVec4(1.0f - hpRatio, hpRatio, 0.2f, 1.0f);
    ImGui::TextColored(hpColor, "HP: %d", playerHP);

    const float colMaterials = 120.f * uiScale;
    const float colScore = 280.f * uiScale;
    const float colWave = 420.f * uiScale;
    const float colTimer = 550.f * uiScale;

    ImGui::SameLine(colMaterials);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Materials: %d",
                       materials);

    ImGui::SameLine(colScore);
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "Score: %d", score);

    ImGui::SameLine(colWave);
    ImGui::TextColored(ImVec4(0.9f, 0.4f, 1.0f, 1.0f), "Wave: %d", currentWave);

    ImGui::SameLine(colTimer);
    if (!waveInProgress) {
      ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Next wave in: %.1fs",
                         std::max(0.0f, waveTimer));
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Enemies: %d / %d",
                         enemiesSpawnedInWave, cfg.enemiesPerWave);
    }

    ImGui::End();

    // Prawy panel: budowa wież — auto wysokość (ikony w całości, bez obcinania)
    ImGui::SetNextWindowPos(ImVec2((float)fbW - towerPanelW, 0));
    ImGui::SetNextWindowSize(ImVec2(towerPanelW, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(12.f * uiScale, 14.f * uiScale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(8.f * uiScale, 10.f * uiScale));
    ImGui::Begin("TowerBuild", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_AlwaysAutoResize);

    {
      ImDrawList *dlBar = ImGui::GetWindowDrawList();
      ImVec2 wp = ImGui::GetWindowPos();
      ImVec2 ws = ImGui::GetWindowSize();
      dlBar->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y),
                           IM_COL32(18, 20, 32, 235));
      const float sepTh = std::max(1.f, 1.25f * uiScale);
      dlBar->AddLine(wp, ImVec2(wp.x, wp.y + ws.y), IM_COL32(90, 110, 160, 255),
                     sepTh);
    }

    ImGui::TextUnformatted("Build");
    ImGui::Spacing();

    const char *towerTips[3] = {"Basic tower", "Laser tower", "Mortar"};

    const float btnW = ImGui::GetContentRegionAvail().x;
    for (int t = 0; t < 3; ++t) {
      if (t > 0)
        ImGui::Dummy(ImVec2(0.f, 4.f * uiScale));

      ImGui::PushID(t);
      ImGui::InvisibleButton("tower", ImVec2(btnW, towerBtnH));
      const bool hovered = ImGui::IsItemHovered();
      const bool sel = (selectedTowerBuild == t);
      if (ImGui::IsItemClicked()) {
        selectedTowerBuild = (sel ? -1 : t);
        selectedPlacedTowerIndex = -1; // Deselect placed tower when entering build mode
      }

      ImDrawList *dl = ImGui::GetWindowDrawList();
      const ImVec2 rmin = ImGui::GetItemRectMin();
      const ImVec2 rmax = ImGui::GetItemRectMax();
      const ImVec2 center((rmin.x + rmax.x) * 0.5f, (rmin.y + rmax.y) * 0.5f);
      const float slotR = std::min(rmax.x - rmin.x, rmax.y - rmin.y) * 0.32f;

      if (sel)
        dl->AddRect(rmin, rmax, IM_COL32(255, 210, 90, 255),
                    6.f * uiScale * 0.25f, 0, borderSel);
      else if (hovered)
        dl->AddRect(rmin, rmax, IM_COL32(140, 180, 220, 255),
                    6.f * uiScale * 0.25f, 0, borderHover);
      else
        dl->AddRect(rmin, rmax, IM_COL32(55, 62, 88, 220),
                    6.f * uiScale * 0.25f, 0, borderNorm);

      DrawTowerShapeIcon(dl, center, slotR, t, iconLineThick);

      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        int cost = (t == 0)   ? cfg.towerBasicCost
                   : (t == 1) ? cfg.towerLaserCost
                              : cfg.towerMortarCost;
        ImGui::SetTooltip("%s\nCost: %d", towerTips[t], cost);
      }

      ImGui::PopID();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);

    // Menu pauzy (Escape) — maksymalizacja okna bez nagłego skoku dt przy
    // zmianie rozmiaru
    if (gamePaused) {
      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(ImVec2((float)fbW, (float)fbH));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
      ImGui::Begin("##PauseDim", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoBringToFrontOnFocus |
                       ImGuiWindowFlags_NoInputs);
      {
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 s = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + s.x, p.y + s.y), IM_COL32(5, 8, 20, 170));
      }
      ImGui::End();
      ImGui::PopStyleVar();

      ImVec2 pmCenter((float)fbW * 0.5f, (float)fbH * 0.5f);
      ImGui::SetNextWindowPos(pmCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGui::Begin("PauseMenu", nullptr,
                   ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_AlwaysAutoResize |
                       ImGuiWindowFlags_NoMove);

      ImGui::TextUnformatted("PAUSED");
      ImGui::Separator();
      if (ImGui::Button("Resume", ImVec2(220.f * uiScale, 0.f)))
        gamePaused = false;

      const int maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
      if (maximized) {
        if (ImGui::Button("Restore window", ImVec2(220.f * uiScale, 0.f)))
          glfwRestoreWindow(window);
      } else {
        if (ImGui::Button("Maximize window", ImVec2(220.f * uiScale, 0.f)))
          glfwMaximizeWindow(window);
      }

      if (ImGui::Button("Quit game", ImVec2(220.f * uiScale, 0.f)))
        glfwSetWindowShouldClose(window, true);

      ImGui::End();
    }
    
    // Tower Menu
    if (selectedPlacedTowerIndex != -1 && selectedPlacedTowerIndex < (int)placedTowers.size() && !gameOver) {
        Tower& tw = placedTowers[selectedPlacedTowerIndex];
        int baseCost = (tw.type == 0) ? cfg.towerBasicCost : ((tw.type == 1) ? cfg.towerLaserCost : cfg.towerMortarCost);
        float baseDamage = (tw.type == 0) ? cfg.towerBasicDamage : ((tw.type == 1) ? cfg.towerLaserDamageBase : cfg.towerMortarDamage);
        
        float currentDamage = baseDamage * (1.0f + tw.upgradeLevel * 1.0f);
        int upgradeCost = baseCost * 2;
        int totalSpent = baseCost;
        for (int lvl = 0; lvl < tw.upgradeLevel; lvl++) totalSpent += baseCost * 2;
        int sellRefund = (int)(totalSpent * 0.5f);
        
        const char* typeName = (tw.type == 0) ? "Basic Tower" : ((tw.type == 1) ? "Laser Tower" : "Mortar");
        
        ImGui::SetNextWindowPos(ImVec2(10.f * uiScale, (float)fbH - 200.f * uiScale), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300.f * uiScale, 0.f));
        ImGui::Begin("TowerMenu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
        
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s (Lvl %d)", typeName, tw.upgradeLevel + 1);
        ImGui::Separator();
        ImGui::Text("Damage: %.1f", currentDamage);
        
        ImGui::Dummy(ImVec2(0, 5));
        
        if (tw.upgradeLevel < 4) { // Max level is 5, meaning upgradeLevel max is 4
            char upText[64];
            snprintf(upText, sizeof(upText), "Upgrade (Cost: %d)", upgradeCost);
            bool cantAfford = materials < upgradeCost;
            if (cantAfford) ImGui::BeginDisabled();
            if (ImGui::Button(upText, ImVec2(-1, 0))) {
                materials -= upgradeCost;
                tw.upgradeLevel++;
            }
            if (cantAfford) ImGui::EndDisabled();
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Max Level Reached");
        }
        
        ImGui::Dummy(ImVec2(0, 5));
        
        char sellText[64];
        snprintf(sellText, sizeof(sellText), "Sell (Refund: %d)", sellRefund);
        if (ImGui::Button(sellText, ImVec2(-1, 0))) {
            materials += sellRefund;
            levelMap.grid[tw.x][tw.z].hasTower = false;
            placedTowers.erase(placedTowers.begin() + selectedPlacedTowerIndex);
            selectedPlacedTowerIndex = -1;
        }
        
        ImGui::End();
    } else if (selectedPlacedTowerIndex >= (int)placedTowers.size()) {
        selectedPlacedTowerIndex = -1; // Safety: index went out of bounds
    }

    // Ekran Game Over
    if (gameOver) {
      ImVec2 center((float)fbW * 0.5f, (float)fbH * 0.5f);
      ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGui::Begin("GameOver", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_AlwaysAutoResize |
                       ImGuiWindowFlags_NoCollapse);

      ImGui::Dummy(ImVec2(0, 10));
      ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "   GAME OVER   ");
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0, 5));
      ImGui::Text("Final Score: %d", score);
      ImGui::Dummy(ImVec2(0, 10));

      ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Sprzątanie Dear ImGui
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

void scroll_callback(GLFWwindow * /*window*/, double /*xoffset*/, double yoffset) {
  if (ImGui::GetIO().WantCaptureMouse)
    return;
  camera.ProcessMouseScroll((float)yoffset);
}

// Zmiana rozmiaru okienka
void framebuffer_size_callback(GLFWwindow * /*window*/, int width, int height) {
  glViewport(0, 0, width, height);
}
