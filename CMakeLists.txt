cmake_minimum_required(VERSION 3.15)
project(SpaceGlowTD)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pobranie zewnętrznych bibliotek (FetchContent jest darmowe i świetne do niezależności projektu)
include(FetchContent)

# 1. Pobieranie GLFW (do okna i contextu)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. Pobieranie GLM (biblioteka matematyczna - headers only)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

# 3. Pobieranie Dear ImGui (UI do HUD, menu itp.)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
)
FetchContent_MakeAvailable(imgui)

# Pliki źródłowe Dear ImGui (brak natywnego CMakeLists.txt - ręczna lista)
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Ustawienie prekompilowanych plików GLAD (znajdują się w folderze /extern/glad)
set(GLAD_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/glad/include")
set(GLAD_SOURCES "${CMAKE_SOURCE_DIR}/extern/glad/src/glad.c")

# Pliki źródłowe naszego modułu 1 kroku
set(SRCS
    src/main.cpp
    src/Shader.cpp
    src/Camera.cpp
    src/Map.cpp
    ${GLAD_SOURCES}
    ${IMGUI_SOURCES}
)

# Aplikacja końcowa wykonywalna
add_executable(${PROJECT_NAME} ${SRCS})

# Podlinkowanie folderów z nagłówkami
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${GLAD_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Podlinkowanie bibliotek bazowych (działanie między systemowe Linux/Windows)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glm::glm)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl X11 pthread)
endif()

# Skopiowanie shaderów do folderu build, by program uruchamiany bezpośrednio tam widział pliki
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
