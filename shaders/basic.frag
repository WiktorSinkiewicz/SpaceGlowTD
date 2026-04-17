#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

// Kolor obiektu ustawiany per-blok z C++
uniform vec3 objectColor;
// Pozycja kamery (do obliczeń odbicia lustrzanego)
uniform vec3 viewPos;

// ========================
// Światło Kierunkowe (Słońce) - oświetla całą scenę równomiernie z jednego kierunku
// ========================
struct DirLight {
    vec3 direction;
    vec3 color;
};
uniform DirLight dirLight;

// ========================
// Punktowe Źródła Światła (Pociski, Eksplozje)
// Każdy odpalony pocisk będzie osobnym PointLight w tej tablicy
// ========================
#define MAX_POINT_LIGHTS 32

struct PointLight {
    vec3 position;
    vec3 color;
    float constant;   // Stała tłumienia (zazwyczaj 1.0)
    float linear;     // Liniowy spadek jasności z dystansem
    float quadratic;  // Kwadratowy spadek (realistyczna fizyka światła)
};
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;

// ========================
// Blinn-Phong: Światło kierunkowe
// ========================
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);

    // Składowa rozpraszania (diffuse) - im bardziej powierzchnia skierowana na światło, tym jaśniej
    float diff = max(dot(normal, lightDir), 0.0);

    // Składowa lustrzana (specular) - Blinn-Phong: wektor połówkowy zamiast odbicia
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // Składowa otoczenia (ambient) - minimalne oświetlenie by nic nie było czarne
    vec3 ambient  = 0.12 * light.color;
    vec3 diffuse  = diff * light.color;
    vec3 specular = 0.4 * spec * light.color;

    return ambient + diffuse + specular;
}

// ========================
// Blinn-Phong: Światło punktowe (Pociski)
// ========================
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular (wyższy wykładnik = ostrzejszy odblask, pociski świecą mocno)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);

    // Tłumienie (attenuation) - światło słabnie z kwadratem odległości
    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 diffuse  = diff * light.color;
    vec3 specular = 0.6 * spec * light.color;

    return (diffuse + specular) * attenuation;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // 1. Oświetlenie kierunkowe (scena bazowa)
    vec3 result = calcDirLight(dirLight, norm, viewDir) * objectColor;

    // 2. Punktowe światła pocisków (dynamiczne, dodawane w runtime)
    for (int i = 0; i < numPointLights; i++) {
        result += calcPointLight(pointLights[i], norm, FragPos, viewDir) * objectColor;
    }

    FragColor = vec4(result, 1.0);
}
