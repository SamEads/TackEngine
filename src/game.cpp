#include <iostream>
#include "game.h"

#include <ProcessInfo.h>
#include <SystemInformation.h>

ProcessInfo process;
SystemInformation sys_info;

void Game::initializeLua(LuaState& L, const std::filesystem::path& assets) {
    lua_getglobal(L, ENGINE_ENV);
    
        // WINDOW
        lua_newtable(L);
            lua_pushcfunction(L, [](lua_State* L) -> int {
                const char* caption = luaL_checkstring(L, 1);
                Game::get().window->setTitle(caption);
                return 0;
            });
            lua_setfield(L, -2, "set_caption");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                float width = luaL_checknumber(L, 1);
                float height = luaL_checknumber(L, 2);
                Game::get().window->setSize({ static_cast<unsigned int>(width), static_cast<unsigned int>(height) });
                return 0;
            });
            lua_setfield(L, -2, "set_size");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                float size = Game::get().window->getSize().x;
                lua_pushnumber(L, size);
                return 1;
            });
            lua_setfield(L, -2, "get_width");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                float size = Game::get().window->getSize().y;
                lua_pushnumber(L, size);
                return 1;
            });
            lua_setfield(L, -2, "get_height");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                auto displaySize = sf::VideoMode::getDesktopMode().size;
                auto windowSize = Game::get().window->getSize();
                Game::get().window->setPosition({
                    static_cast<int>((displaySize.x / 2) - (windowSize.x / 2)),
                    static_cast<int>((displaySize.y / 2) - (windowSize.y / 2))
                });
                return 0;
            });
            lua_setfield(L, -2, "center");
        lua_setfield(L, -2, "window");

        // RUNTIME
        lua_newtable(L);
            lua_pushcfunction(L, [](lua_State* L) -> int {
                float tickSpeed = luaL_checknumber(L, 1);
                Game::get().timer.setTickRate(tickSpeed);
                return 0;
            });
            lua_setfield(L, -2, "set_tick_rate");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                auto memory = process.GetMemoryUsage() / 1'000;
                lua_pushnumber(L, memory);
                return 1;
            });
            lua_setfield(L, -2, "get_memory");
            
            lua_newtable(L);
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* key = lua_tostring(L, 2);

                    if (strcmp(key, "fps") == 0) {
                        lua_pushnumber(L, Game::get().fps);
                        return 1;
                    }

                    lua_pushnil(L);
                    return 1;
                });
                lua_setfield(L, -2, "__index");
            lua_setmetatable(L, -2);

        lua_setfield(L, -2, "runtime");

    lua_pop(L, 1);
}