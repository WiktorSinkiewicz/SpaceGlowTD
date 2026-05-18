#include "UI.h"
#include "GameState.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// Bazowy rozmiar interfejsu
static constexpr float kUiRefShortSide = 640.f;

static ImGuiStyle g_uiStyleBaseline;
static float s_uiLastScale = -1.f;
static bool s_uiBaselineCaptured = false;

// Rysuje ikonę wieży: 0 = prostokąt, 1 = trójkąt, 2 = sześciokąt
static void DrawTowerShapeIcon(ImDrawList *dl, ImVec2 center, float radius,
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

void renderUI(GLFWwindow* window, int fbW, int fbH, float aspect,
              Camera& camera,
              std::vector<Enemy>& enemies,
              std::vector<Tower>& placedTowers,
              Map& levelMap) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    // Skalowanie UI
    const float uiScale = std::clamp(
        std::min((float)fbW, (float)fbH) / kUiRefShortSide, 0.85f, 3.0f);

    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = uiScale;

    ImGuiStyle &style = ImGui::GetStyle();
    if (!s_uiBaselineCaptured) {
      g_uiStyleBaseline = style;
      s_uiBaselineCaptured = true;
    }
    if (s_uiLastScale < 0.f) {
      style = g_uiStyleBaseline;
      style.ScaleAllSizes(uiScale);
      s_uiLastScale = uiScale;
    } else if (std::fabs(uiScale - s_uiLastScale) > 0.001f) {
      const float ratio = uiScale / s_uiLastScale;
      style.ScaleAllSizes(ratio);
      s_uiLastScale = uiScale;
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

    // Paski zdrowia wrogów
    ImDrawList *bgDrawList = ImGui::GetBackgroundDrawList();
    glm::mat4 viewM = camera.GetViewMatrix();
    glm::mat4 projM =
        glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 100.0f);

    for (const Enemy &e : enemies) {
      if (e.hp < e.maxHp && e.hp > 0.0f) {
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

    // Górny pasek HUD
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)fbW - towerPanelW, hudTopH));
    ImGui::Begin("HUD", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBackground);

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

    // Prawy panel budowy wież
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
        selectedPlacedTowerIndex = -1;
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

    // Menu pauzy
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
        
        if (tw.upgradeLevel < 4) {
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
        selectedPlacedTowerIndex = -1;
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
}
