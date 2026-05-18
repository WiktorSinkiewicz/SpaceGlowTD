# SpaceGlowTD

Gra 3D Tower Defense w C++ i OpenGL. Mapa generowana proceduralnie, trzy typy wież (basic, laser, moździerz), system ulepszeń i sprzedaży, efekty cząsteczkowe eksplozji oraz dynamiczne oświetlenie punktowe.

## Funkcje

- Proceduralnie generowana mapa z terenem o różnych wysokościach
- 3 wieże: Basic (homing), Laser (ładujący się DPS), Mortar (trajektoria paraboliczna z AoE)
- System ulepszeń wież (do poziomu 5) i sprzedaż za 50% kosztów
- Eksplozje oparte na systemie cząsteczkowym z dynamicznym oświetleniem
- Fale wrogów ze skalowaniem trudności
- UI budowy i zarządzania wieżami (ImGui)

## Uruchomienie

1. Stwórz folder budowania: `mkdir build && cd build`
2. Wygeneruj pliki projektu: `cmake ..`
3. Skompiluj projekt: `cmake --build .`
4. Uruchom wygenerowany plik wykonywalny wewnątrz folderu `build`.
<img width="1569" height="1143" alt="image" src="https://github.com/user-attachments/assets/80b6de23-0bee-4177-aa61-059295e98cdf" />
