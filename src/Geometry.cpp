#include "Geometry.h"

std::vector<float> generatePrismVertices(int sides) {
  std::vector<float> vertices;
  const float PI = 3.14159265f;
  float angleStep = 2.0f * PI / sides;

  float angleOffset = (sides == 4) ? (PI / 4.0f) : 0.0f;
  float radius = (sides == 4) ? (0.5f * sqrt(2.0f)) : 0.5f;

  // Compute total perimeter for proper UV tiling
  float perimeter = 0.0f;
  std::vector<float> edgeLengths(sides);
  for (int i = 0; i < sides; ++i) {
    float a1 = i * angleStep + angleOffset;
    float a2 = (i + 1) * angleStep + angleOffset;
    float x1 = radius * cos(a1), z1 = radius * sin(a1);
    float x2 = radius * cos(a2), z2 = radius * sin(a2);
    float dx = x2 - x1, dz = z2 - z1;
    edgeLengths[i] = sqrt(dx * dx + dz * dz);
    perimeter += edgeLengths[i];
  }

  float uAccum = 0.0f;
  // UV tiling: 2 repeats around the perimeter, 2 repeats vertically
  float uScale = 2.0f;
  float vScale = 2.0f;

  for (int i = 0; i < sides; ++i) {
    float a1 = i * angleStep + angleOffset;
    float a2 = (i + 1) * angleStep + angleOffset;

    float x1 = radius * cos(a1);
    float z1 = radius * sin(a1);
    float x2 = radius * cos(a2);
    float z2 = radius * sin(a2);

    glm::vec3 normal =
        glm::normalize(glm::vec3((x1 + x2) / 2.0f, 0.0f, (z1 + z2) / 2.0f));

    // UV mapping for side faces — tile the brick texture
    float u0 = (uAccum / perimeter) * uScale;
    float u1 = ((uAccum + edgeLengths[i]) / perimeter) * uScale;
    float vBot = 0.0f;
    float vTop = vScale;
    uAccum += edgeLengths[i];

    // Side face (2 triangles) — vertex format: pos(3) + normal(3) + uv(2)
    vertices.insert(vertices.end(),
                    {x1, 0.0f, z1, normal.x, normal.y, normal.z, u0, vBot});
    vertices.insert(vertices.end(),
                    {x1, 1.0f, z1, normal.x, normal.y, normal.z, u0, vTop});
    vertices.insert(vertices.end(),
                    {x2, 0.0f, z2, normal.x, normal.y, normal.z, u1, vBot});

    vertices.insert(vertices.end(),
                    {x2, 0.0f, z2, normal.x, normal.y, normal.z, u1, vBot});
    vertices.insert(vertices.end(),
                    {x1, 1.0f, z1, normal.x, normal.y, normal.z, u0, vTop});
    vertices.insert(vertices.end(),
                    {x2, 1.0f, z2, normal.x, normal.y, normal.z, u1, vTop});

    // Top face — simple radial UV for cap
    vertices.insert(vertices.end(), {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f});
    vertices.insert(vertices.end(), {x2, 1.0f, z2, 0.0f, 1.0f, 0.0f,
                                     0.5f + x2, 0.5f + z2});
    vertices.insert(vertices.end(), {x1, 1.0f, z1, 0.0f, 1.0f, 0.0f,
                                     0.5f + x1, 0.5f + z1});

    // Bottom face
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f});
    vertices.insert(vertices.end(), {x1, 0.0f, z1, 0.0f, -1.0f, 0.0f,
                                     0.5f + x1, 0.5f + z1});
    vertices.insert(vertices.end(), {x2, 0.0f, z2, 0.0f, -1.0f, 0.0f,
                                     0.5f + x2, 0.5f + z2});
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
