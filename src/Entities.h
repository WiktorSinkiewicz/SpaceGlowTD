#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdlib>

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
  glm::vec3 velocity;
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

inline void spawnExplosion(const glm::vec3& pos, const glm::vec3& color, float maxLife, float baseScale, std::vector<Particle>& particles, std::vector<FlashLight>& flashes) {
    FlashLight flash;
    flash.pos = pos;
    flash.color = color;
    flash.lifeTime = maxLife * 0.5f;
    flash.maxLifeTime = maxLife * 0.5f;
    flashes.push_back(flash);

    int numParticles = 20 + rand() % 15;
    for (int j = 0; j < numParticles; j++) {
        Particle pt;
        pt.pos = pos;
        
        float r1 = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float r2 = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float r3 = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        
        glm::vec3 dir = glm::normalize(glm::vec3(r1, r2 + 0.5f, r3));
        
        float speed = 3.0f + ((float)rand() / RAND_MAX) * 5.0f;
        pt.velocity = dir * speed;
        
        pt.maxLifeTime = maxLife * (0.5f + ((float)rand() / RAND_MAX) * 0.5f);
        pt.lifeTime = pt.maxLifeTime;
        
        pt.color = color;
        if (rand() % 3 == 0) {
            pt.color = glm::mix(color, glm::vec3(1.0f, 1.0f, 1.0f), 0.5f);
        }
        
        pt.initialScale = baseScale * (0.2f + ((float)rand() / RAND_MAX) * 0.3f);
        particles.push_back(pt);
    }
}
