#pragma once

#include <SFML/Graphics.hpp>
#include "luainc.h"

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
	void initializeLua(LuaState Lua);
};