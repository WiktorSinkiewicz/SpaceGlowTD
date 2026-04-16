#version 330 core
layout (location = 0) in vec3 aPos;

// Uniformy dla podpiecia macierzy OpenGL (Transformacje 3D)
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Mnożenie macierzy zachodzi od prawej do lewej
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
