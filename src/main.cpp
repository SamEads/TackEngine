#ifdef MIN_EXAMPLE

#include <SFML/Graphics.hpp>
#ifdef USE_LUA_JIT
extern "C" {
    #include <luajit.h>
}
#endif
#include <sol/sol.hpp>

#include "game.h"
#include "sprite.h"
int main() {
    auto& game = Game::get();

    sol::state& lua = game.lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::jit, sol::lib::ffi);
    
    game.window = std::make_unique<sf::RenderWindow>(sf::VideoMode({ 256 * 3, 224 * 3 }), "TackEngine");
    game.window->setVerticalSyncEnabled(false);

    game.consoleRenderer = std::make_unique<sf::RenderTexture>(sf::Vector2u { 256, 224 });
    game.currentRenderer = game.consoleRenderer.get();

    sol::table gametbl = lua.create_named_table("game");

    SpriteManager::get().initializeLua(lua, "assets");

    auto objectBase = lua.create_table_with(
        "x", 0,
        "y", 0,
        "hspeed", 0,
        "vspeed", 0,
        "xprevious", 0,
        "yprevious", 0,
        "sprite_index", sol::lua_nil
    );
    
    auto res = lua.load(
R"(
local object = ...

local object_reference = {}
function object_reference:new(object)
    local mt = {
        __index = function(self, k)
            return object[k]
        end,
        __newindex = function(self, k, v)
            object[k] = v
        end
    }
    return setmetatable({
        object = object,
        id = 0
    }, mt)
end

function object:extend()
    local o = setmetatable({}, self)
    self.__index = self
    return o
end

function object:new()
    local o = setmetatable({}, self)
    self.__index = self
    local ref = object_reference:new(o)
    return ref
end

function object:draw()
    gfx.draw_sprite(self.spr, 0, self.x, self.y)
end
)"
    )(objectBase);

    if (!res.valid()) {
        sol::error e = res;
        std::cout << e.what() << "\n";
    }

    lua["Guy"] = objectBase;
    auto guys = lua.create_table();
    lua["guys"] = guys;

    lua.safe_script_file("assets/scripts/test.lua");
    gametbl["init"].get<sol::function>()();

    auto updatePositions = lua.load(
R"(
    local tbl = ...
    local count = #tbl
    for i = 1, count do
        local t = tbl[i]
        t.xprevious = t.x
        t.yprevious = t.y
    end
)"
    );

    while (game.window->isOpen()) {
        while (const std::optional event = game.window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                game.window->close();
            }
        }

        updatePositions(guys);

        gametbl["update"].get<sol::function>()();
        game.consoleRenderer->clear(sf::Color::White);
        gametbl["draw"].get<sol::function>()();
        /*
        auto& spr = SpriteManager::get().sprites["spr_mario_small_idle"];
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 100; ++j) {
                spr.draw(*Game::get().currentRenderer, { i * 2.5f, j * 2.5f });
            }
        }
        */
        game.consoleRenderer->display();

        game.window->clear();
        sf::Sprite renderSprite(game.consoleRenderer->getTexture());
        renderSprite.setPosition({ 150, 150 });
        game.window->draw(renderSprite);
        game.window->display();
    }
}

#else

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
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::jit, sol::lib::ffi);
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
    game.timer.setTickRate(60);

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

    lua["game"]["get_memory"] = [&process]() {
        auto memory = process.GetMemoryUsage() / 1'000;
        return memory;
    };

    SoundManager& sndMgr = SoundManager::get();
    sndMgr.thread = std::thread(&SoundManager::update, &sndMgr);
    game.window = std::make_unique<sf::RenderWindow>(sf::VideoMode({ 256 * 3, 224 * 3 }), "TackEngine");
    auto& window = game.window;

    game.consoleRenderer = std::make_unique<sf::RenderTexture>(sf::Vector2u { 256, 224 });

    lua["game"]["init"](lua["game"]);

    sf::Clock clock;
    int fps = 0;
    int frame = 0;
    while (window->isOpen()) {
        if (game.queuedRoom) {
            game.room = std::move(game.queuedRoom);
            game.queuedRoom.reset();
        }
        while (const std::optional event = window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window->close();
            }
        }

        game.timer.update();
        
        const int ticks = game.timer.getTickCount();
        for (int i = 0; i < ticks; ++i) {
            game.getKVP("step").as<sol::function>()(game);
            Keys::get().update();
            if (game.room) {
                game.room->update();
            }

            if (Keys::get().pressed(sf::Keyboard::Scancode::F5)) {
                const sf::Texture& t = game.consoleRenderer.get()->getTexture();
                sf::Image i = t.copyToImage();
                bool saved = i.saveToFile("_.png");
            }
        }

        game.currentRenderer = game.consoleRenderer.get();
        game.consoleRenderer->clear();
        if (game.room) {
            float alpha = game.timer.getAlpha();
            game.room->draw(alpha);
        }
        game.consoleRenderer->display();

        window->clear();

        const auto dispSize = window->getSize();
        sf::View view(sf::FloatRect{ { 0, 0 }, { (float)dispSize.x, (float)dispSize.y } });
        view.setCenter({ dispSize.x / 2.0f, dispSize.y / 2.0f });
        window->setView(view);

        sf::Sprite renderSprite(game.consoleRenderer->getTexture());
        sf::Vector2u gameSize = game.consoleRenderer->getSize();

        if (game.letterbox && dispSize.x > gameSize.x && dispSize.y > gameSize.y) {
            float scaleX = floorf(dispSize.x / (float)gameSize.x);
            float scaleY = floorf(dispSize.y / (float)gameSize.y);
            float scale = std::max(1.0f, std::min(scaleX, scaleY));
    
            renderSprite.setScale({ scale, scale });
    
            float offsetX = floorf((dispSize.x - (gameSize.x * scale)) / 2.f);
            float offsetY = floorf((dispSize.y - (gameSize.y * scale)) / 2.f);
            renderSprite.setPosition({ offsetX, offsetY });
        }
        else {
            const float ratio = gameSize.x / (float)gameSize.y;

            if (dispSize.x >= dispSize.y * ratio) {
                float scaleY = dispSize.y / (float)gameSize.y;

                renderSprite.setScale({ scaleY, scaleY });

                float scaledW = gameSize.x * scaleY;
                float offsetX = (dispSize.x - scaledW) / 2.0f;
                renderSprite.setPosition({ offsetX, 0.0f });
            }
            else {
                float scaleX = dispSize.x / (float)gameSize.x;

                renderSprite.setScale({ scaleX, scaleX });

                float scaledH = gameSize.y * scaleX;
                float offsetY = (dispSize.y - scaledH) / 2.0f;
                renderSprite.setPosition({ 0.0f, offsetY });
            }
        }

        auto it = ShaderManager::get().shaders.find("shd_palette");
        if (it != ShaderManager::get().shaders.end()) {
            Shader& shader = it->second;
            auto& palSpr = SpriteManager::get().sprites["spr_pal_all"];
            
            auto uvs = palSpr.getUVs();
            auto texels = palSpr.getTexelSize();

            shader.baseShader.setUniform("u_pal",                           palSpr.texture);
            shader.baseShader.setUniform("u_pal_uvs", sf::Glsl::Vec4(std::get<0>(uvs), std::get<1>(uvs), std::get<2>(uvs), std::get<3>(uvs)));
            shader.baseShader.setUniform("u_pal_texel_size", sf::Glsl::Vec2(std::get<0>(texels), std::get<1>(texels)));
            static float d = 0;
            if (Keys::get().pressed(sf::Keyboard::Scancode::D)) {
                d = (d == 1) ? 0 : 1;
            }
            shader.baseShader.setUniform("u_pal_index", 2.0f);

            window->draw(renderSprite, &shader.baseShader);

            window->display();
        }

        float delta = clock.restart().asSeconds();
        game.fps = 1.f / delta;

        ++frame;
    }

    ObjectManager::get().baseClasses.clear();
    TilesetManager::get().tilesets.clear();
    SpriteManager::get().sprites.clear();
}

#endif