#include <iostream>
#include "shader.h"
#include "sprite.h"
#include "game.h"

// TODO

void ShaderManager::initializeLua(LuaState L) {
    lua_getglobal(L, ENGINE_ENV);
        lua_newtable(L);

#define CREATE_SHADER(arg1, arg2) \
lua_newtable(L); \
    Shader* lShader = new(lua_newuserdata(L, sizeof(Shader))) Shader(); \
        bool loaded = lShader->baseShader.loadFromMemory(arg1, arg2); \
        if (loaded) {} \
    lua_setfield(L, -2, "__cpp_ptr");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                const char* frag = luaL_checkstring(L, 1);
                CREATE_SHADER(frag, sf::Shader::Type::Fragment)
                return 1;
            });
            lua_setfield(L, -2, "add_fragment");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                const char* vert = luaL_checkstring(L, 1);
                CREATE_SHADER(vert, sf::Shader::Type::Vertex)
                return 1;
            });
            lua_setfield(L, -2, "add_vertex");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                CREATE_SHADER(luaL_checkstring(L, 1), luaL_checkstring(L, 2))
                return 1;
            });
            lua_setfield(L, -2, "add");

#undef CREATE_SHADER

            lua_pushcfunction(L, [](lua_State* L) -> int {
                int baseArgs = lua_gettop(L);

                Shader* ptr = lua_toclass<Shader>(L, 1);
                sf::Shader& baseShader = ptr->baseShader;
                const char* uniform = luaL_checkstring(L, 2);

                if (lua_istable(L, 3)) {
                    lua_pushstring(L, "__cpp_ptr");
                    lua_rawget(L, 3);
                    if (!lua_isnil(L, -1)) {
                        GFX::Sprite* ind = lua_toclassfromref<GFX::Sprite>(L, 3);
                        baseShader.setUniform(uniform, ind->texture);
                        return 0;
                    }
                    lua_pop(L, 1);

                    int l = lua_rawlen(L, 3);

                    if (l == 2) {
                        float v1 = lua_tonumbertable(L, 3, 1);
                        float v2 = lua_tonumbertable(L, 3, 2);
                        baseShader.setUniform(uniform, sf::Glsl::Vec2 { v1, v2 });
                        return 0;
                    }
                    // Vector 3
                    if (l == 3) {
                        float v1 = lua_tonumbertable(L, 3, 1);
                        float v2 = lua_tonumbertable(L, 3, 2);
                        float v3 = lua_tonumbertable(L, 3, 3);
                        baseShader.setUniform(uniform, sf::Glsl::Vec3 { v1, v2, v3 });
                        return 0;
                    }
                    // Vector 4
                    if (l == 4) {
                        float v1 = lua_tonumbertable(L, 3, 1);
                        float v2 = lua_tonumbertable(L, 3, 2);
                        float v3 = lua_tonumbertable(L, 3, 3);
                        float v4 = lua_tonumbertable(L, 3, 4);
                        baseShader.setUniform(uniform, sf::Glsl::Vec4 { v1, v2, v3, v4 });
                        return 0;
                    }
                }
                // Bool
                if (lua_isboolean(L, 3)) {
                    baseShader.setUniform(uniform, lua_toboolean(L, 3));
                    return 0;
                }
                // Float
                if (lua_isnumber(L, 3)) {
                    baseShader.setUniform(uniform, static_cast<float>(lua_tonumber(L, 3)));
                    return 0;
                }

                return 0;
            });
            lua_setfield(L, -2, "set_uniform");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                if (lua_isnil(L, 1)) {
                    Game::get().currentShader = nullptr;
                    return 0;
                }
                Game::get().currentShader = &lua_toclass<Shader>(L, 1)->baseShader;
                return 0;
            });
            lua_setfield(L, -2, "bind");

        lua_setfield(L, -2, "shader");
    lua_pop(L, 1);
}