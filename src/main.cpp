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
#include <ProcessInfo.h>
#include <SystemInformation.h>
#define GMC_EMBEDDED
#define GMCONVERT_IMPLEMENTATION
#include "util/gmconvert.h"

using namespace nlohmann;

void InitializeLuaEnvironment(sol::state& lua) {
    std::filesystem::path assets = Game::get().assetsFolder;

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
}

int main() {
    Game& game = Game::get();
    sol::state& lua = game.lua;

    if (std::filesystem::exists("assets/managed/gmconvert.lua")) {
        auto res = lua.safe_script_file("assets/managed/gmconvert.lua");
        if (!res.valid()) {
            sol::error e = res;
            std::cout << e.what() << "\n";
        }

        std::filesystem::path p = std::filesystem::path(lua["project_directory"].get<std::string>());
        GMConvert(p, "assets/managed");
    }

    InitializeLuaEnvironment(lua);

    ProcessInfo process;
    SystemInformation sys_info;

    game.engineEnv["game"]["get_memory"] = [&process]() {
        auto memory = process.GetMemoryUsage() / 1'000;
        return memory;
    };

    SoundManager& sndMgr = SoundManager::get();
    sndMgr.thread = std::thread(&SoundManager::update, &sndMgr);
    game.window = std::make_unique<sf::RenderWindow>(sf::VideoMode({ 256 * 3, 224 * 3 }), "TackEngine");
    auto& window = game.window;

    game.currentRenderer = nullptr;

    game.timer.setTickRate(60);
    game.engineEnv["game"]["init"](game);

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
            game.getKVP("step").as<sol::function>()(game);
            Keys::get().update(game.window->hasFocus());
        }

        float alpha = game.timer.getAlpha();

        window->clear();

        const auto dispSize = window->getSize();
        sf::View view(sf::FloatRect{ { 0, 0 }, { (float)dispSize.x, (float)dispSize.y } });
        view.setCenter({ dispSize.x / 2.0f, dispSize.y / 2.0f });
        window->setView(view);
        
        auto& drawFunc = game.getKVP("draw");
        if (drawFunc != sol::lua_nil) {
            drawFunc.as<sol::function>()(game, alpha);
        }

        window->display();

        float delta = clock.restart().asSeconds();
        game.fps = 1.f / delta;

        ++frame;
    }

    ObjectManager::get().gmlObjects.clear();
    TilesetManager::get().tilesets.clear();
    SpriteManager::get().sprites.clear();
}