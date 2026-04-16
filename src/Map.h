#pragma once

#include <vector>
#include <glm/glm.hpp>

enum TileType {
    TILE_EMPTY,
    TILE_PATH,
    TILE_PORTAL,
    TILE_BASE
};

struct Tile {
    int x;
    int z;
    float height;
    TileType type;
};

class Map {
public:
    int width;
    int height;
    std::vector<std::vector<Tile>> grid;
    std::vector<glm::ivec2> path;

    Map(int w, int h);

    void generateTerrain();
    void generateWindingPath(int minLen, int maxLen);

private:
    bool findAlternativePath(int i, int j, const std::vector<glm::ivec2>& path, std::vector<glm::ivec2>& newSegment);
};
