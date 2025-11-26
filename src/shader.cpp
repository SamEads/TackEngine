#include <iostream>
#include "shader.h"
#include "sprite.h"
#include "game.h"

void ShaderManager::initializeLua(sol::state& lua) {
    sol::table engineEnv = lua["TE"];
    sol::table shaderModule = engineEnv.create_named("shader");

    shaderModule["add_fragment"] = [&](const std::string& fragment) {
        Shader s = Shader ();
        bool loaded = s.baseShader.loadFromMemory(fragment, sf::Shader::Type::Fragment);
        if (loaded) { }
        return s;
    };

    shaderModule["add_vertex"] = [&](const std::string& fragment) {
        Shader s = Shader ();
        bool loaded = s.baseShader.loadFromMemory(fragment, sf::Shader::Type::Vertex);
        if (loaded) { }
        return s;
    };

    shaderModule["add"] = [&](const std::string& vertex, const std::string& fragment) {
        Shader s = Shader ();
        bool loaded = s.baseShader.loadFromMemory(vertex, fragment);
        if (loaded) { }
        return s;
    };

    shaderModule["set_uniform"] = [&](Shader* shader, const std::string& uniform, sol::object data) {
        setUniform(shader, uniform, data);
    };

    shaderModule["bind"] = [&](sol::object shader) {
        if (shader == sol::lua_nil) {
            Game::get().currentShader = nullptr;
            return;
        }
        
        if (shader.is<Shader*>()) {
            Shader* s = shader.as<Shader*>();
            Game::get().currentShader = &s->baseShader;
        }
    };
}

void ShaderManager::setUniform(Shader *shader, const std::string &uniform, sol::object data) {
    if (data.is<bool>()) {
        shader->baseShader.setUniform(uniform, data.as<bool>());
    }
    else if (data.is<float>()) {
        shader->baseShader.setUniform(uniform, data.as<float>());
    }
    else if (data.is<SpriteIndex*>()) {
        SpriteIndex* spr = data.as<SpriteIndex*>();
        shader->baseShader.setUniform(uniform, spr->texture);
    }
    else if (data.is<sol::table>()) {
        auto tbl = data.as<sol::table>();
        if (tbl.size() == 2) {
            shader->baseShader.setUniform(uniform, sf::Glsl::Vec2 { tbl.get<float>(1), tbl.get<float>(2) });
        }
        else if (tbl.size() == 3) {
            shader->baseShader.setUniform(uniform, sf::Glsl::Vec3 { tbl.get<float>(1), tbl.get<float>(2), tbl.get<float>(3) });
        }
        else if (tbl.size() == 4) {
            shader->baseShader.setUniform(uniform, sf::Glsl::Vec4 { tbl.get<float>(1), tbl.get<float>(2), tbl.get<float>(3), tbl.get<float>(4) });
        }
    }
}
