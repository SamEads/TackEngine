#ifdef USE_LUA_JIT
extern "C" {
#include <luajit.h>
}
#endif
#include <fstream>
#include "vendor/json.hpp"
#include "sprite.h"
#include "object.h"
#include "tileset.h"
#include "game.h"
#include "room.h"
#include "keys.h"
#include "shader.h"
#include "sound.h"
#include "font.h"

#define GMC_EMBEDDED
#define GMCONVERT_IMPLEMENTATION
#include "util/gmconvert.h"

using namespace nlohmann;

int setLuaPath(LuaState& L, const char* path) {
    lua_getglobal(L, "package");
        lua_getfield(L, -1, "path"); // get field path
            std::string cur_path = lua_tostring(L, -1);
            // adding ;LibPath (so that it has an extra zone to scan in)
            cur_path.append(";");
            cur_path.append(path);
        lua_pop(L, 1); // path field
    lua_pushstring( L, cur_path.c_str() ); // new path field
        lua_setfield( L, -2, "path" ); // replace path in global package
    lua_pop( L, 1 );
    return 0;
}

namespace LuatMathExt {
    static int Round (lua_State* L) {
        lua_pushnumber(L, std::round(luaL_checknumber(L, 1)));
        return 1;
    }

    static int Ceil (lua_State* L) {
        lua_pushnumber(L, std::ceil(luaL_checknumber(L, 1)));
        return 1;
    }

    static int Sign (lua_State* L) {
        double num = luaL_checknumber(L, 1);
        
        int numval = 0;
        if (num > 0) numval = 1;
        else if (num < 0) numval = -1;

        lua_pushinteger(L, numval);
        return 1;
    }

    static int Lerp (lua_State* L) {
        double a = luaL_checknumber(L, 1);
        double b = luaL_checknumber(L, 2);
        double t = luaL_checknumber(L, 3);
        double lerp = a + (b - a) * t;
        lua_pushnumber(L, lerp);
        return 1;
    }

    static int Clamp (lua_State* L) {
        double v = luaL_checknumber(L, 1);
        double min = luaL_checknumber(L, 2);
        double max = luaL_checknumber(L, 3);
        double clamp = std::max(min, std::min(v, max));
        lua_pushnumber(L, clamp);
        return 1;
    }

    static int Intersects (lua_State* L) {
        lua_geti(L, 1, 1);
        float ax = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_geti(L, 1, 2);
        float ay = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_geti(L, 1, 3);
        float aw = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_geti(L, 1, 4);
        float ah = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_geti(L, 2, 1);
        float bx = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_geti(L, 2, 2);
        float by = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_geti(L, 2, 3);
        float bw = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_geti(L, 2, 4);
        float bh = lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        sf::FloatRect ra = { { ax, ay }, { aw, ah } };
        sf::FloatRect rb = { { bx, by }, { bw, bh } };
        bool result = ra.findIntersection(rb).has_value();
        lua_pushboolean(L, result);
        return 1;
    }

    static int PointDistance (lua_State* L) {
        double x1 = luaL_checknumber(L, 1);
        double y1 = luaL_checknumber(L, 2);
        double x2 = luaL_checknumber(L, 3);
        double y2 = luaL_checknumber(L, 4);
        double distance = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2) * 1.0);
        lua_pushnumber(L, distance);
        return 1;
    }
}

void InitializeLuaEnvironment(LuaState& L) {
    {
        lua_State** rawStatePtr = &L.l;
        (*rawStatePtr) = luaL_newstate();
        luaL_openlibs(*rawStatePtr);

        LuaState* statePtr = &L;
        lua_pushlightuserdata(L, (void*)statePtr);
        lua_setfield(L, LUA_REGISTRYINDEX, "__cppstate");
    }

    std::filesystem::path assets = Game::get().assetsFolder;
    std::filesystem::path scriptsPath = (std::filesystem::path(assets.string()) / "scripts" / "?.lua");
    setLuaPath(L, scriptsPath.string().c_str());

    // small math library
    lua_getglobal(L, "math");
    lua_pushcfunction(L, LuatMathExt::Round);             lua_setfield(L, -2, "round");
    lua_pushcfunction(L, LuatMathExt::Sign);              lua_setfield(L, -2, "sign");
    lua_pushcfunction(L, LuatMathExt::Lerp);              lua_setfield(L, -2, "lerp");
    lua_pushcfunction(L, LuatMathExt::Ceil);              lua_setfield(L, -2, "ceil");
    lua_pushcfunction(L, LuatMathExt::PointDistance);     lua_setfield(L, -2, "point_distance");
    lua_pushcfunction(L, LuatMathExt::Clamp);             lua_setfield(L, -2, "clamp");
    lua_pushcfunction(L, LuatMathExt::Intersects);        lua_setfield(L, -2, "intersects");
    lua_pop(L, 1);

    lua_newtable(L);
    lua_setglobal(L, ENGINE_ENV);
    
    Room::initializeLua(L, assets);
    TilesetManager::get().initializeLua(L, assets);
    GFX::initializeLua(L, assets);
    ObjectManager::get().initializeLua(L, assets);
    FontManager::get().initializeLua(L, assets);
    SoundManager::get().initializeLua(L, assets);
    MusicManager::get().initializeLua(L, assets);
    ShaderManager::get().initializeLua(L);
    Keys::get().initializeLua(L);
    Game::get().initializeLua(L, assets);
}

int main() {
    Game& game = Game::get();

    if (std::filesystem::exists("assets/managed/gmconvert.lua")) {
        lua_State* pleasechangethis = luaL_newstate();
        luaL_dofile(pleasechangethis, "assets/managed/gmconvert.lua");
        lua_getglobal(pleasechangethis, "project_directory");
        std::filesystem::path p = std::filesystem::path(std::string(lua_tostring(pleasechangethis, -1)));
        GMConvert(p, "assets/managed");
        lua_close(pleasechangethis);
    }

    InitializeLuaEnvironment(game.L);

    SoundManager& sndMgr = SoundManager::get();
    sndMgr.thread = std::thread(&SoundManager::update, &sndMgr);
    game.window = std::make_unique<sf::RenderWindow>(sf::VideoMode({ 640, 480 }), "TackEngine");
    auto& window = game.window;

    game.currentRenderer = nullptr;

    game.timer.setTickRate(60);
    auto lua = game.L;

    luaL_dofile(game.L, "assets/scripts/game.lua");
    
    lua_getglobal(lua, ENGINE_ENV); // -1 TE
    
    lua_getfield(lua, -1, "init"); // -1 Step -2 TE
    if (!lua_isnil(lua, -1)) {
        lua_lazycall(lua, 0, 0);
    }
    else {
        lua_pop(lua, 1); // pop init null
    }
    lua_pop(lua, 1); // pop te

    sf::Clock clock;
    sf::Clock stupid;
    int fps = 0, frame = 0;
    while (window->isOpen()) {
        while (const std::optional event = window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window->close();
            }
        }

        game.timer.update();
        
        const int ticks = game.timer.getTickCount();
        if (ticks > 0) {
            lua_getglobal(lua, ENGINE_ENV); // -1 TE
            for (int i = 0; i < ticks; ++i) {
                lua_getfield(lua, -1, "step"); // -1 Step -2 TE
                lua_lazycall(lua, 0, 0);
                
                Keys::get().update(game.window->hasFocus());
            }
            lua_pop(lua, 1); // bal
        }

        float alpha = game.timer.getAlpha();

        window->clear();

        const auto dispSize = window->getSize();
        sf::View view(sf::FloatRect{ { 0, 0 }, { (float)dispSize.x, (float)dispSize.y } });
        view.setCenter({ dispSize.x / 2.0f, dispSize.y / 2.0f });
        window->setView(view);

        lua_getglobal(lua, ENGINE_ENV); // TE
            lua_getfield(lua, -1, "draw"); // Draw function, te
                lua_pushnumber(lua, alpha); // Alpha, Draw function, TE
                lua_lazycall(lua, 1, 0); // TE
        lua_pop(lua, 1); // =

        window->display();

        float delta = clock.restart().asSeconds();
        game.fps = 1.f / delta;

        ++frame;

        if (stupid.getElapsedTime().asSeconds() >= 1) {
            stupid.restart();
        }
    }

    lua_close(game.L);

    TilesetManager::get().tilesets.clear();
    GFX::sprites.clear();
}