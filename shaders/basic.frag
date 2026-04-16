#version 330 core
out vec4 FragColor;

// Uniform koloru do zmiany barwy na bieżąco w pętli dla każdego bloku z osobna
uniform vec3 objectColor;

void main()
{
    FragColor = vec4(objectColor, 1.0f);
}
