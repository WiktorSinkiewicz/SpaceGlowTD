#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#include "Shader.h"
#include "Camera.h"
#include "Map.h"

struct Enemy {
    int pathIndex;
    float progress;
    glm::vec3 currentPos;
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Odsunięty mocno widok RTS, wyśrodkowany nad mapą 20x20!
// (środek mapy to X=10, Z=10). Y=15 to wysokość latania nad ziemią.
Camera camera(glm::vec3(10.0f, 15.0f, 22.0f));

float deltaTime = 0.0f; 
float lastFrame = 0.0f; 

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "SpaceGlowTD - Voxel Terrain", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Blad przy ladowaniu okna GLFW!" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Kursor pozostaje włączony, gdyż jest on RTS/TD grą, w KROKU 4 dodamy Raycasting.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Blad przy inicjalizacji GLAD!" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    
    // Odpalenie culling (nie renderuj ścian z tyłu kostki względem kamery, da nam to FPS w 400 klockach bez problemu)
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    Shader basicShader("shaders/basic.vert", "shaders/basic.frag");

    // Pozycja (XYZ) + Normalna (XYZ) = 6 floatów na wierzchołek, 36 wierzchołków = 216 floatów
    float vertices[] = {
        // Tylna sciana - normalna (0, 0, -1)
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        // Przednia sciana - normalna (0, 0, 1)
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        // Lewa sciana - normalna (-1, 0, 0)
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        // Prawa sciana - normalna (1, 0, 0)
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
        // Dolna sciana - normalna (0, -1, 0)
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        // Gorna sciana - normalna (0, 1, 0)
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Atrybut 0: Pozycja (3 floaty, stride 6 = przeskok do następnego wierzchołka)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Atrybut 1: Normalna (3 floaty, offset 3 = za pozycją)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // KROK 2 - Generowanie struktury gry
    Map levelMap(20, 20);
    levelMap.generateTerrain();
    
    // Generowanie Bazy, Portalu i mutowanie proceduralnej ewolucyjnej Ścieżki z limitem w okolicy 40-52 klatek do celów Tabu Search
    levelMap.generateWindingPath(40, 52);

    std::vector<Enemy> enemies;
    float spawnTimer = 0.0f;
    float spawnRate = 1.5f;
    float enemySpeed = 1.25f; // Szybkość przejazdu UFO (na sekundę przelatuje ~1.25 bloczki)

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // System Spawnowania
        spawnTimer += deltaTime;
        if (spawnTimer >= spawnRate) {
            spawnTimer = 0.0f;
            if (levelMap.path.size() > 1) {
                Enemy e;
                e.pathIndex = 0;
                e.progress = 0.0f;
                e.currentPos = glm::vec3(levelMap.path[0].x, 0.4f, levelMap.path[0].y); // Y z path używane jest jako Z
                enemies.push_back(e);
            }
        }

        // Płynna Fizyka i Logika Trasy (UFO)
        for (size_t i = 0; i < enemies.size(); i++) {
            Enemy& e = enemies[i];
            e.progress += enemySpeed * deltaTime;
            
            if (e.progress >= 1.0f) {
                e.progress -= 1.0f;
                e.pathIndex++;
            }
            
            // Check Base collision
            if (e.pathIndex >= (int)levelMap.path.size() - 1) {
                enemies.erase(enemies.begin() + i);
                i--;
                continue; // Usunięto ufo, powrót do kolejnego na liście
            }
            
            // Liniowa Interpolacja Mix w przestrzeni kafelków 3D 
            glm::vec2 p1 = glm::vec2((float)levelMap.path[e.pathIndex].x, (float)levelMap.path[e.pathIndex].y);
            glm::vec2 p2 = glm::vec2((float)levelMap.path[e.pathIndex + 1].x, (float)levelMap.path[e.pathIndex + 1].y);
            
            glm::vec2 current2D = glm::mix(p1, p2, e.progress);
            // Y w grze rygujemy na płaskie 0.6f by najeżdżały na kostki
            e.currentPos = glm::vec3(current2D.x, 0.6f, current2D.y);
        }

        glClearColor(0.02f, 0.05f, 0.10f, 1.0f); // Bardzo mroczne, głębokie kosmiczne tło
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        basicShader.use();

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        basicShader.setMat4("projection", projection);
        glm::mat4 view = camera.GetViewMatrix();
        basicShader.setMat4("view", view);

        // Oświetlenie: pozycja kamery do obliczeń refleksji lustrzanej
        basicShader.setVec3("viewPos", camera.Position);

        // Światło kierunkowe (kosmiczne słońce) - pada z góry-prawej pod kątem
        basicShader.setVec3("dirLight.direction", glm::vec3(-0.3f, -1.0f, -0.4f));
        basicShader.setVec3("dirLight.color", glm::vec3(0.85f, 0.85f, 0.95f));

        // Na razie 0 świateł punktowych - pociski będą je dodawać dynamicznie w runtime
        basicShader.setInt("numPointLights", 0);

        glBindVertexArray(VAO);
        
        // Renderowanie 400 Kafelków Mapy
        for(int x = 0; x < levelMap.width; x++) {
            for(int z = 0; z < levelMap.height; z++) {
                Tile& t = levelMap.grid[x][z];
                
                glm::mat4 model = glm::mat4(1.0f);
                
                // Przestawiamy każdy sześcian by spoczywał na dole X i Z. Ujemny punkt obrotu Y ustawi go równo z Ziemią mimo skalowania.
                model = glm::translate(model, glm::vec3(x * 1.0f, t.height / 2.0f, z * 1.0f)); 
                // Skalowanie wysokości (by udawało wyrastające voxelowe skały). Odrobina luki by widzieć Grid (skaluje na np. 0.95 w boki)
                model = glm::scale(model, glm::vec3(0.95f, t.height, 0.95f)); 
                
                basicShader.setMat4("model", model);
                
                // Zarządzanie barwami pod mapę
                glm::vec3 objColor;
                if(t.type == TILE_PORTAL) objColor = glm::vec3(0.9f, 0.2f, 0.1f); // Czerwony Portal
                else if(t.type == TILE_BASE) objColor = glm::vec3(0.1f, 0.9f, 0.2f); // Zielona Baza
                else if(t.type == TILE_PATH) objColor = glm::vec3(0.0f, 0.8f, 0.9f); // Ścieżka UFO mocno Neonowa
                else {
                    // Góry i nierówności szumu nabierają koloru fioletowego bazującego na ich własnej wysokości ułożenia!
                    objColor = glm::vec3(0.15f, 0.1f * t.height, 0.3f + (0.2f * t.height));
                }
                
                basicShader.setVec3("objectColor", objColor);

                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }
        
        // Renderowanie Systemu Najeźdzców UFO po narysowaniu mapy
        for (const Enemy& e : enemies) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, e.currentPos);
            model = glm::scale(model, glm::vec3(0.4f, 0.4f, 0.4f)); // Mniejsze sześciany uciekają z Bazy
            basicShader.setMat4("model", model);
            
            // Pomaracznowy kolor wroga UFO
            basicShader.setVec3("objectColor", glm::vec3(1.0f, 0.5f, 0.0f)); 
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

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
void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
}
