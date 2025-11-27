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

#ifdef __WIN32
#include <ProcessInfo.h>
#include <SystemInformation.h>
#endif

#define GMC_EMBEDDED
#define GMCONVERT_IMPLEMENTATION
#include "util/gmconvert.h"

using namespace nlohmann;

int setLuaPath(LuaState& L, const char* path) {
    lua_getglobal( L, "package" );
    lua_getfield( L, -1, "path" ); // get field "path" from table at top of stack (-1)
    std::string cur_path = lua_tostring( L, -1 ); // grab path string from top of stack
    cur_path.append( ";" ); // do your path magic here
    cur_path.append( path );
    lua_pop( L, 1 ); // get rid of the string on the stack we just pushed on line 5
    lua_pushstring( L, cur_path.c_str() ); // push the new one
    lua_setfield( L, -2, "path" ); // set the field "path" in table at -2 with value at top of stack
    lua_pop( L, 1 ); // get rid of package table from top of stack
    return 0; // all done!
}

namespace LMATH {
    static int ROUND (lua_State* L) {
        double num = luaL_checknumber(L, 1);
        lua_pushnumber(L, std::round(num));
        return 1;
    }
    static int CEIL (lua_State* L) {
        double num = luaL_checknumber(L, 1);
        lua_pushnumber(L, std::ceil(num));
        return 1;
    }
    static int SIGN (lua_State* L) {
        double num = luaL_checknumber(L, 1);
        
        int numval = 0;
        if (num > 0) numval = 1;
        else if (num < 0) numval = -1;

        lua_pushinteger(L, numval);
        return 1;
    }
    static int LERP (lua_State* L) {
        double a = luaL_checknumber(L, 1);
        double b = luaL_checknumber(L, 2);
        double t = luaL_checknumber(L, 3);
        double lerp = a + (b - a) * t;
        lua_pushnumber(L, lerp);
        return 1;
    }
    static int CLAMP (lua_State* L) {
        double v = luaL_checknumber(L, 1);
        double min = luaL_checknumber(L, 2);
        double max = luaL_checknumber(L, 3);
        double clamp = std::max(min, std::min(v, max));
        lua_pushnumber(L, clamp);
        return 1;
    }
    static int INTERSECTS (lua_State* L) {
        // a
        lua_geti(L, 1, 1);
        float ax = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        
        lua_geti(L, 1, 2);
        float ay = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        
        lua_geti(L, 1, 3);
        float aw = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        
        lua_geti(L, 1, 4);
        float ah = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        
        // b
        lua_geti(L, 2, 1);
        float bx = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        
        lua_geti(L, 2, 2);
        float by = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        
        lua_geti(L, 2, 3);
        float bw = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        
        lua_geti(L, 2, 4);
        float bh = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        
        sf::FloatRect ra = { { ax, ay }, { aw, ah } };
        sf::FloatRect rb = { { bx, by }, { bw, bh } };
        bool result = ra.findIntersection(rb).has_value();
        lua_pushboolean(L, result);
        return 1;
    }
    static int POINT_DISTANCE (lua_State* L) {
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
    std::filesystem::path assets = Game::get().assetsFolder;

    {
        lua_State** rawStatePtr = &L.l;
        (*rawStatePtr) = luaL_newstate();
        luaL_openlibs(*rawStatePtr);
    }

    LuaState* statePtr = &L;
    lua_pushlightuserdata(L.l, (void*)statePtr);
    lua_setfield(L.l, LUA_REGISTRYINDEX, "__cppstate");

    std::filesystem::path scriptsPath = (std::filesystem::path(assets.string()) / "scripts" / "?.lua");
    setLuaPath(L, scriptsPath.string().c_str());

    L.pushGlobal("math");
        L.setFunction("round", LMATH::ROUND);
        L.setFunction("sign", LMATH::SIGN);
        L.setFunction("lerp", LMATH::LERP);
        L.setFunction("ceil", LMATH::CEIL);
        L.setFunction("point_distance", LMATH::POINT_DISTANCE);
        L.setFunction("clamp", LMATH::CLAMP);
        L.setFunction("intersects", LMATH::INTERSECTS);
    L.pop(1);

    L.newTable();
    L.setGlobal("TE");

    L.pushGlobal("TE");
        L.setFunction("object_create", [] (lua_State* L) -> int {
            // BaseObject* baseObject = static_cast<BaseObject*>(lua_newuserdata(L, sizeof(BaseObject)));
            return 0;
        });
    L.pop(1);
    
    L.doString("print(TE)");
    L.doString("print(TE.object_create)");
    
    Room::initializeLua(L, assets);
    L.checkpoint();

#ifdef OLD

#ifdef USE_LUA_JIT
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::jit);
#else
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table);
#endif

    std::filesystem::path scriptsPath = (std::filesystem::path(assets.string()) / "scripts" / "?.lua");
    lua["package"]["path"] = scriptsPath.string();

    // Initialize math library extension
    sol::table math = lua["math"];
    math["round"] = std::roundf;
    math["sign"] = signum;
    math["lerp"] = lerp;
    math["floor"] = std::floorf;
    math["ceil"] = std::ceilf;
    math["point_distance"] = PointDistance;
    math["clamp"] = [](float v, float min, float max) {
        return std::max(min, std::min(v, max));
    };
    math["intersects"] = [](sol::table a, sol::table b) {
        sf::FloatRect ra = { { a.get<float>(1), a.get<float>(2) }, { a.get<float>(3), a.get<float>(4) } };
        sf::FloatRect rb = { { b.get<float>(1), b.get<float>(2) }, { b.get<float>(3), b.get<float>(4) } };
        return ra.findIntersection(rb).has_value();
    };

    Game::get().engineEnv = lua.create_named_table("TE");

    Room::initializeLua(lua, assets);
    TilesetManager::get().initializeLua(lua, assets);
    SpriteManager::get().initializeLua(lua, assets);
    ObjectManager::get().initializeLua(lua, assets);
    FontManager::get().initializeLua(lua, assets);
    SoundManager::get().initializeLua(lua, assets);
    MusicManager::get().initializeLua(lua, assets);
    ShaderManager::get().initializeLua(lua);
    Keys::get().initializeLua(lua);
    Game::get().initializeLua(lua, assets);

#endif
}

int main() {
    Game& game = Game::get();

    /*
    if (std::filesystem::exists("assets/managed/gmconvert.lua")) {
        auto res = lua.safe_script_file("assets/managed/gmconvert.lua");
        if (!res.valid()) {
            sol::error e = res;
            std::cout << e.what() << "\n";
        }

        std::filesystem::path p = std::filesystem::path(lua["project_directory"].get<std::string>());
        GMConvert(p, "assets/managed");
    }
    */

    InitializeLuaEnvironment(game.L);

#ifdef __WIN32
    ProcessInfo process;
    SystemInformation sys_info;
#endif

#ifdef __WIN32
    game.engineEnv["game"]["get_memory"] = [&process]() {
        auto memory = process.GetMemoryUsage() / 1'000;
        return memory;
    };
#else
    /*
    game.engineEnv["game"]["get_memory"] = []() {
        return 0;
    };
    */
#endif

    SoundManager& sndMgr = SoundManager::get();
    sndMgr.thread = std::thread(&SoundManager::update, &sndMgr);
    game.window = std::make_unique<sf::RenderWindow>(sf::VideoMode({ 256 * 3, 224 * 3 }), "TackEngine");
    auto& window = game.window;

    game.currentRenderer = nullptr;

    game.timer.setTickRate(60);
    // game.engineEnv["game"]["init"](game);

    sf::Clock clock;
    int fps = 0, frame = 0;
    while (window->isOpen()) {
        while (const std::optional event = window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window->close();
            }
        }

        game.timer.update();
        
        const int ticks = game.timer.getTickCount();
        for (int i = 0; i < ticks; ++i) {
            // game.getKVP("step").as<sol::function>()(game);
            Keys::get().update(game.window->hasFocus());
        }

        float alpha = game.timer.getAlpha();

        window->clear();

        const auto dispSize = window->getSize();
        sf::View view(sf::FloatRect{ { 0, 0 }, { (float)dispSize.x, (float)dispSize.y } });
        view.setCenter({ dispSize.x / 2.0f, dispSize.y / 2.0f });
        window->setView(view);
        
        /*
        sol::object drawFunc = game.getKVP("draw");
        if (drawFunc != sol::lua_nil) {
            drawFunc.as<sol::function>()(game, alpha);
        }
        */

        window->display();

        float delta = clock.restart().asSeconds();
        game.fps = 1.f / delta;

        ++frame;
    }

    lua_close(game.L.l);

    ObjectManager::get().gmlObjects.clear();
    TilesetManager::get().tilesets.clear();
    SpriteManager::get().sprites.clear();
}