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

#include "Camera.h"
#include "Map.h"
#include "Shader.h"


struct Enemy {
  int pathIndex;
  float progress;
  glm::vec3 currentPos;
};

struct Tower {
  int x;
  int z;
  int type; // 0: Basic, 1: Laser, 2: Mortar
};

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
    ImVec2 a(center.x - radius * 1.05f, center.y - radius * 0.65f);
    ImVec2 b(center.x + radius * 1.05f, center.y + radius * 0.65f);
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

// Stan gry
int playerHP = 10;
int materials = 100;
int score = 0;
bool gameOver = false;
bool gamePaused = false;

// -1 = brak wyboru, 0 = zwykła wieża, 1 = laser, 2 = moździerz (do przyszłego
// stawiania na mapie)
int selectedTowerBuild = -1;

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
  std::vector<Tower> placedTowers;

  // KROK 2 - Generowanie struktury gry
  Map levelMap(20, 20);
  levelMap.generateTerrain();

  // Generowanie Bazy, Portalu i mutowanie proceduralnej ewolucyjnej Ścieżki z
  // limitem w okolicy 40-52 klatek do celów Tabu Search
  levelMap.generateWindingPath(40, 52);

  std::vector<Enemy> enemies;
  float spawnTimer = 0.0f;
  float spawnRate = 1.5f;
  float enemySpeed =
      1.25f; // Szybkość przejazdu UFO (na sekundę przelatuje ~1.25 bloczki)

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
        !gameOver) {
      Tile &t = levelMap.grid[hoverX][hoverZ];
      if (selectedTowerBuild != -1 && t.type == TILE_EMPTY && !t.hasTower) {
        int cost = (selectedTowerBuild == 0)
                       ? 10
                       : ((selectedTowerBuild == 1) ? 20 : 30);
        if (materials >= cost) {
          materials -= cost;
          t.hasTower = true;
          placedTowers.push_back({hoverX, hoverZ, selectedTowerBuild});
          selectedTowerBuild = -1; // Deselect after build
        }
      }
    }
    mouseLeftPrev = mouseLeftDown;

    // Logika gry zatrzymuje się po Game Over lub w pauzie
    if (!gameOver && !gamePaused) {

      // System Spawnowania
      spawnTimer += deltaTime;
      if (spawnTimer >= spawnRate) {
        spawnTimer = 0.0f;
        if (levelMap.path.size() > 1) {
          Enemy e;
          e.pathIndex = 0;
          e.progress = 0.0f;
          e.currentPos =
              glm::vec3(levelMap.path[0].x, 0.4f,
                        levelMap.path[0].y); // Y z path używane jest jako Z
          enemies.push_back(e);
        }
      }

      // Płynna Fizyka i Logika Trasy (UFO)
      for (size_t i = 0; i < enemies.size(); i++) {
        Enemy &e = enemies[i];
        e.progress += enemySpeed * deltaTime;

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

    // Światło kierunkowe (kosmiczne słońce) - pada z góry-prawej pod kątem
    basicShader.setVec3("dirLight.direction", glm::vec3(-0.3f, -1.0f, -0.4f));
    basicShader.setVec3("dirLight.color", glm::vec3(0.85f, 0.85f, 0.95f));

    // Na razie 0 świateł punktowych - pociski będą je dodawać dynamicznie w
    // runtime
    basicShader.setInt("numPointLights", 0);

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
          objColor = glm::vec3(0.9f, 0.2f, 0.1f); // Czerwony Portal
        else if (t.type == TILE_BASE)
          objColor = glm::vec3(0.1f, 0.9f, 0.2f); // Zielona Baza
        else if (t.type == TILE_PATH)
          objColor = glm::vec3(0.0f, 0.8f, 0.9f); // Ścieżka UFO mocno Neonowa
        else {
          // Góry i nierówności szumu nabierają koloru fioletowego bazującego na
          // ich własnej wysokości ułożenia!
          objColor =
              glm::vec3(0.15f, 0.1f * t.height, 0.3f + (0.2f * t.height));
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

      glm::vec3 tColor = (tow.type == 0) ? glm::vec3(0.9f, 0.9f, 0.9f)
                                         : // White/Silver
                             (tow.type == 1) ? glm::vec3(0.1f, 0.9f, 0.9f)
                                             :            // Cyan
                             glm::vec3(1.0f, 0.8f, 0.1f); // Yellow/Gold
      basicShader.setVec3("objectColor", tColor);
      basicShader.setFloat("objectAlpha", 1.0f);

      glBindVertexArray(towerVAO[tow.type]);
      glDrawArrays(GL_TRIANGLES, 0, towerVertexCount[tow.type]);
    }

    // Renderowanie "Ducha" wieży podczas budowania
    if (selectedTowerBuild != -1 && hoverValid) {
      Tile &t = levelMap.grid[hoverX][hoverZ];
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model,
                             glm::vec3(hoverX * 1.0f, t.height, hoverZ * 1.0f));
      model = glm::scale(model, glm::vec3(0.6f, 1.2f, 0.6f));
      basicShader.setMat4("model", model);

      glm::vec3 tColor = (selectedTowerBuild == 0) ? glm::vec3(0.9f, 0.9f, 0.9f)
                         : (selectedTowerBuild == 1)
                             ? glm::vec3(0.1f, 0.9f, 0.9f)
                             : glm::vec3(1.0f, 0.8f, 0.1f);
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

    const float colMaterials = 280.f * uiScale;
    const float colScore = 600.f * uiScale;
    ImGui::SameLine(colMaterials);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Materials: %d",
                       materials);

    ImGui::SameLine(colScore);
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "Score: %d", score);

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
      if (ImGui::IsItemClicked())
        selectedTowerBuild = (sel ? -1 : t);

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

      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", towerTips[t]);

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
}

// Zmiana rozmiaru okienka
void framebuffer_size_callback(GLFWwindow * /*window*/, int width, int height) {
  glViewport(0, 0, width, height);
}
