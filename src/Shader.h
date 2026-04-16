#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>

// Klasa shadera enkapsuluje ładowanie, kompilację oraz ustawianie stałych parametrów na karcie graficznej.
class Shader {
public:
    // Identyfikator programu shadera
    unsigned int ID;

    // Pobiera i buduje shader na z dwóch plików
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader(); // Pamiętaj o usunięciu zasobów, OOP wymaga porządku

    // Aktywacja programu cieniującego przed rysowaniem
    void use() const;

    // Funkcje do definiowania zmiennych we shaderze (tzw. uniformy) z poziomu C++
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;

private:
    // Sprawdza logi jeżeli kompilacja w C++ / C zakończy się błędem
    void checkCompileErrors(unsigned int shader, std::string type) const;
};
