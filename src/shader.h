#pragma once

#include <SFML/Graphics.hpp>
#include <sol/sol.hpp>

class Shader {
public:
    sf::Shader baseShader;
};

class ShaderManager {
public:
    std::unordered_map<std::string, Shader> shaders;
    static ShaderManager& get() {
        static ShaderManager sm;
        return sm;
    }
	void initializeLua(sol::state& lua);
    void setUniform(Shader* shader, const std::string& uniform, sol::object data);
};