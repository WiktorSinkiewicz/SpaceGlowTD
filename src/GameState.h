#pragma once

#include "GameConfig.h"

extern GameConfig cfg;

extern int playerHP;
extern int materials;
extern int score;
extern bool gameOver;
extern bool gamePaused;

extern int currentWave;
extern int enemiesSpawnedInWave;
extern float waveTimer;
extern bool waveInProgress;

extern int selectedTowerBuild; // -1 = none, 0/1/2 = basic/laser/mortar
extern int selectedPlacedTowerIndex;
