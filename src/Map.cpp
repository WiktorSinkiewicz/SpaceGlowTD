#include "Map.h"
#include <iostream>
#include <random>
#include <algorithm>
#include <queue>
#include "../extern/FastNoiseLite.h"

Map::Map(int w, int h) : width(w), height(h) {
    grid.resize(width, std::vector<Tile>(height));
    for (int x = 0; x < width; ++x) {
        for (int z = 0; z < height; ++z) {
            grid[x][z] = {x, z, 0.0f, TILE_EMPTY};
        }
    }
}

void Map::generateTerrain() {
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(4);
    
    std::random_device rd;
    noise.SetSeed(rd());
    noise.SetFrequency(0.35f);

    for (int x = 0; x < width; ++x) {
        for (int z = 0; z < height; ++z) {
            float n = noise.GetNoise((float)x, (float)z);
            n = (n + 1.0f) * 0.5f; 
            // Zbalansowana optymalna wysokość gór dla lepszej estetyki kanionów w TD (Zmniejszono z 6.0 na 2.5)
            grid[x][z].height = n * 2.5f; 
        }
    }
}

bool Map::findAlternativePath(int i, int j, const std::vector<glm::ivec2>& path, std::vector<glm::ivec2>& newSegment) {
    std::vector<std::vector<bool>> staticPath(width, std::vector<bool>(height, false));
    std::vector<std::vector<bool>> forbidden(width, std::vector<bool>(height, false));

    glm::ivec2 startNode = path[i];
    glm::ivec2 endNode = path[j];

    for(int k = 0; k < path.size(); k++) {
        if (k <= i || k >= j) staticPath[path[k].x][path[k].y] = true;
        else forbidden[path[k].x][path[k].y] = true;
    }

    std::queue<glm::ivec2> q;
    std::vector<std::vector<glm::ivec2>> parent(width, std::vector<glm::ivec2>(height, {-1, -1}));
    
    q.push(startNode);
    parent[startNode.x][startNode.y] = startNode;

    std::vector<glm::ivec2> dirs = {{1,0}, {-1,0}, {0,1}, {0,-1}};
    
    std::random_device rd;
    std::mt19937 gen(rd());

    bool found = false;
    while(!q.empty()) {
        glm::ivec2 curr = q.front();
        q.pop();

        if (curr == endNode) {
            found = true;
            break;
        }

        // Mieszanie dla nieskończonej unikalności
        std::shuffle(dirs.begin(), dirs.end(), gen);

        for(auto& d : dirs) {
            glm::ivec2 nx = curr + d;

            // Granice
            if (nx.x <= 0 || nx.x >= width - 1 || nx.y <= 0 || nx.y >= height - 1) continue;
            // Odwiedzone lub należące do starej kasowanej ścieżki (zabronione by wymusić omijanie dookoła!)
            if (parent[nx.x][nx.y].x != -1) continue;
            if (forbidden[nx.x][nx.y]) continue;

            // Zezwól na połączenie z celem
            if (nx == endNode) {
                parent[nx.x][nx.y] = curr;
                q.push(nx);
                continue;
            }

            // Jeśli to stary mur patha i to nie cel - odrzucamy
            if (staticPath[nx.x][nx.y]) continue;

            // Anty-kolizyjność: Płytek nie może dotykać żadnych elementów statycznej ścieżki za wyjątkiem Startu i Końca do podłączenia
            bool touchesStatic = false;
            for(auto& d2 : dirs) {
                glm::ivec2 nnx = nx + d2;
                if (staticPath[nnx.x][nnx.y]) {
                    if (nnx != startNode && nnx != endNode) {
                        touchesStatic = true;
                        break;
                    }
                }
            }
            if (touchesStatic) continue;

            parent[nx.x][nx.y] = curr;
            q.push(nx);
        }
    }

    if (found) {
        glm::ivec2 curr = endNode;
        while(curr != startNode) {
            newSegment.push_back(curr);
            curr = parent[curr.x][curr.y];
        }
        newSegment.push_back(startNode);
        std::reverse(newSegment.begin(), newSegment.end());
        return true;
    }
    return false;
}

void Map::generateWindingPath(int minLen, int maxLen) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> gridDist(2, width - 3);

    int startX, startZ, endX, endZ;
    do {
        startX = gridDist(gen); startZ = gridDist(gen);
        endX = gridDist(gen); endZ = gridDist(gen);
    } while (std::abs(startX - endX) + std::abs(startZ - endZ) < 6);

    path.clear();
    int cx = startX, cz = startZ;
    path.push_back({cx, cz});
    
    // Klasyczna minimalna długa ścieżka bez zawijasów "L" (Krok 1)
    while(cx != endX || cz != endZ) {
        if (cx != endX) cx += (endX > cx) ? 1 : -1;
        else cz += (endZ > cz) ? 1 : -1;
        path.push_back({cx, cz});
    }

    int iters = 0;
    while(path.size() < maxLen && iters < 3000) {
        iters++;
        
        // Prawdopodobieństwo 5% na akceptacje dobrego wyniku z góry i zatrzymania procesu po wejściu we widełki by nie ciągnąć w nieskończoność do oporu
        if (path.size() >= minLen) {
            std::uniform_int_distribution<> chance(0, 100);
            if (chance(gen) < 8) break;
        }

        std::uniform_int_distribution<> iDist(0, path.size() - 4);
        int i = iDist(gen);
        
        // Maksymalny dystans odcięcia starej trasy + 14 kroków, by węzły miały mocne lokalne mutacje
        int endDist = std::min((int)path.size() - 1, i + 14);
        std::uniform_int_distribution<> jDist(i + 2, endDist);
        int j = jDist(gen);

        std::vector<glm::ivec2> segment;
        // Transformacja lokalna Mutacji. Szuka ominięcia starych klocków by poszukać dłuższej, nowej, legalnej drogi - bez 180-turn i auto-styczności
        if (findAlternativePath(i, j, path, segment)) {
            if (segment.size() > (j - i + 1)) {
                path.erase(path.begin() + i + 1, path.begin() + j);
                path.insert(path.begin() + i + 1, segment.begin() + 1, segment.end() - 1);
            }
        }
    }

    // Adaptacja w strukturę gridu
    for(auto& p : path) {
        grid[p.x][p.y].type = TILE_PATH;
        grid[p.x][p.y].height = 0.1f;
    }
    
    grid[startX][startZ].type = TILE_PORTAL;
    grid[startX][startZ].height = 0.5f;
    grid[endX][endZ].type = TILE_BASE;
    grid[endX][endZ].height = 0.5f;
}
