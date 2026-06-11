#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// Kolor obiektu ustawiany per-blok z C++
uniform vec3 objectColor;
uniform vec3 viewPos;

// Toggle: 1 = sample brickTexture and multiply with objectColor, 0 = use objectColor only
uniform int useTexture;
uniform sampler2D brickTexture;

// Światło Kierunkowe
struct DirLight {
    vec3 direction;
    vec3 color;
};
uniform DirLight dirLight;

// Punktowe Źródła Światła
#define MAX_POINT_LIGHTS 32

struct PointLight {
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;

uniform float objectAlpha = 1.0;

// Obliczenie światła kierunkowego
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 albedo) {
    vec3 lightDir = normalize(-light.direction);
    
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    vec3 ambient  = 0.12 * light.color * albedo;
    vec3 diffuse  = diff * light.color * albedo;
    vec3 specular = 0.4 * spec * light.color; // Czyste odbicie światła

    return ambient + diffuse + specular;
}

// Obliczenie światła punktowego
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo) {
    vec3 lightDir = normalize(light.position - fragPos);
    
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);

    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 diffuse  = diff * light.color * albedo;
    vec3 specular = 0.8 * spec * light.color;

    // Burn-in effect
    float burnFactor = exp(-dist * 4.0);
    vec3 burnGlow = light.color * burnFactor * 1.5;

    return (diffuse + specular + burnGlow) * attenuation;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Determine albedo: multiply texture color with objectColor when texturing is active
    vec3 albedo = objectColor;
    if (useTexture == 1) {
        vec3 texColor = texture(brickTexture, TexCoord).rgb;
        albedo = objectColor * texColor;
    }

    vec3 result = calcDirLight(dirLight, norm, viewDir, albedo);

    for (int i = 0; i < numPointLights; i++) {
        result += calcPointLight(pointLights[i], norm, FragPos, viewDir, albedo);
    }

    FragColor = vec4(result, objectAlpha);
}
