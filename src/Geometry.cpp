#include "Geometry.h"

std::vector<float> generatePrismVertices(int sides) {
  std::vector<float> vertices;
  const float PI = 3.14159265f;
  float angleStep = 2.0f * PI / sides;

  float angleOffset = (sides == 4) ? (PI / 4.0f) : 0.0f;
  float radius = (sides == 4) ? (0.5f * sqrt(2.0f)) : 0.5f;

  for (int i = 0; i < sides; ++i) {
    float a1 = i * angleStep + angleOffset;
    float a2 = (i + 1) * angleStep + angleOffset;

    float x1 = radius * cos(a1);
    float z1 = radius * sin(a1);
    float x2 = radius * cos(a2);
    float z2 = radius * sin(a2);

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
