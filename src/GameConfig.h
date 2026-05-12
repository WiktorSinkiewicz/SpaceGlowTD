#pragma once

#include <glm/glm.hpp>

// --- Game Configuration ---
struct GameConfig {
  int initialPlayerHP = 10;
  int initialMaterials = 100;

  int enemiesPerWave = 5;
  float timeBetweenWaves = 5.0f;
  float spawnRate = 1.0f;

  float initialEnemyHP = 10.0f;
  float enemyHPMultiplierPerWave = 1.35f;
  float enemySpeed = 1.5f;

  int initialEnemyReward = 5;
  int rewardIncreasePerWave = 2;

  // Tower Settings
  int towerBasicCost = 20;
  int towerLaserCost = 30;
  int towerMortarCost = 40;

  float towerBasicDamage = 5.0f;
  float towerBasicRange = 4.0f;
  float heightRangeMultiplier = 0.15f; // +15% zasięgu za każdy blok wysokości
  float towerBasicCooldown = 1.0f;
  float projectileSpeed = 8.0f;

  float towerLaserDamageBase = 15.0f;
  float towerLaserDamageMaxMultiplier = 3.0f;
  float towerLaserChargeTime = 2.0f;
  float towerLaserRange = 6.0f;

  float towerMortarDamage = 20.0f;
  float towerMortarRange = 10.0f;
  float towerMortarCooldown = 3.0f;
  float towerMortarSplashRadius = 2.5f;
  float mortarGravity = 9.81f;
  float mortarHorizontalSpeed = 2.5f;

  // Render & Brightness Settings
  float projectileBrightness = 1.5f;
  float laserBrightness = 1.0f;
  float pointLightBrightnessMultiplier = 0.3f;
  float explosionDurationMortar = 0.5f;
  float explosionDurationEnemy = 0.3f;

  // Colors
  glm::vec3 colorPortal = glm::vec3(0.9f, 0.2f, 0.1f);
  glm::vec3 colorBase = glm::vec3(0.1f, 0.9f, 0.2f);
  glm::vec3 colorPath = glm::vec3(0.0f, 0.8f, 0.9f);
  glm::vec3 colorTerrainLow = glm::vec3(0.15f, 0.0f, 0.3f);
  glm::vec3 colorTerrainHigh = glm::vec3(0.15f, 0.5f, 0.8f);

  glm::vec3 colorExplosionMortar = glm::vec3(1.0f, 0.4f, 0.0f);
  glm::vec3 colorExplosionEnemy = glm::vec3(0.1f, 0.9f, 1.0f);

  glm::vec3 colorTowerBasic = glm::vec3(0.9f, 0.9f, 0.9f);
  glm::vec3 colorTowerLaser = glm::vec3(0.1f, 0.9f, 0.9f);
  glm::vec3 colorTowerMortar = glm::vec3(1.0f, 0.8f, 0.1f);
};
