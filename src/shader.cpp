#include <iostream>
#include "shader.h"
#include "sprite.h"

void ShaderManager::initializeLua(sol::state& lua) {
    lua.create_named_table("shader");

    lua["shader"]["add_fragment"] = [&](const std::string& identifier, const std::string& fragment) {
        Shader& s = this->shaders[identifier];
        bool loaded = s.baseShader.loadFromMemory(fragment, sf::Shader::Type::Fragment);
        if (loaded) { }
        lua[identifier] = &s;
    };

    lua["shader"]["add_vertex"] = [&](const std::string& identifier, const std::string& fragment) {
        Shader& s = this->shaders[identifier];
        bool loaded = s.baseShader.loadFromMemory(fragment, sf::Shader::Type::Vertex);
        if (loaded) { }
        lua[identifier] = &s;
    };

    lua["shader"]["add"] = [&](const std::string& identifier, const std::string& vertex, const std::string& fragment) {
        Shader& s = this->shaders[identifier];
        bool loaded = s.baseShader.loadFromMemory(vertex, fragment);
        if (loaded) { }
        lua[identifier] = &s;
    };

    lua["shader"]["set_uniform"] = [&](Shader* shader, const std::string& uniform, sol::object data) {
        setUniform(shader, uniform, data);
    };

    lua["shader"]["bind"] = [&](sol::object shader) {
        if (shader == sol::lua_nil) {
            sf::Shader::bind(nullptr);
        }
        else {
            if (shader.is<Shader*>()) {
                Shader* s = shader.as<Shader*>();
                sf::Shader::bind(&s->baseShader);
            }
        }
    };
}

void ShaderManager::setUniform(Shader *shader, const std::string &uniform, sol::object data) {
    if (data.is<float>()) {
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
