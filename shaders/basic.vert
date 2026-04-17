#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// Macierze transformacji 3D
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Dane przekazywane do Fragment Shadera dla oświetlenia
out vec3 FragPos;
out vec3 Normal;

void main()
{
    // Pozycja wierzchołka w przestrzeni świata (potrzebna do obliczeń odległości światła)
    FragPos = vec3(model * vec4(aPos, 1.0));
    // Normalna z korekcją na nierównomierne skalowanie (np. kostki o różnej wysokości)
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
