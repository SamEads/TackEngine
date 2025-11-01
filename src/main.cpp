#ifdef USE_LUA_JIT
extern "C" {
#include <luajit.h>
}
#endif
#include <fstream>
#include "vendor/json.hpp"
#include "util/timer.h"
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

    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::jit);
    std::filesystem::path scriptsPath = (std::filesystem::path(assets.string()) / "scripts" / "?.lua");
    lua["package"]["path"] = scriptsPath.string();

    // Initialize math library extension
    sol::table math = lua["math"];
    math["point_distance"] = PointDistance;
    math["sign"] = signum;
    math["lerp"] = lerp;
    math["round"] = std::roundf;
    math["clamp"] = [](float v, float min, float max) {
        return std::max(min, std::min(v, max));
    };
    math["floor"] = std::floorf;
    math["ceil"] = std::ceilf;
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
    sol::state& lua = Game::get().lua;

    auto res = lua.safe_script_file("assets/managed/gmconvert.lua");
    if (!res.valid()) {
        sol::error e = res;
        std::cout << e.what() << "\n";
    }

    std::filesystem::path p = std::filesystem::path(lua["project_directory"].get<std::string>());
    GMConvert(p, "assets/managed");

    InitializeLuaEnvironment(lua);

    ProcessInfo process;
    SystemInformation sys_info;

    lua["game"]["get_memory"] = [&process]() {
        auto memory = process.GetMemoryUsage() / 1'000;
        return memory;
    };

    SoundManager& sndMgr = SoundManager::get();
    sndMgr.thread = std::thread(&SoundManager::update, &sndMgr);
    Game::get().window = std::make_unique<sf::RenderWindow>(sf::VideoMode({ 256 * 3, 224 * 3 }), "TackEngine");
    auto& window = Game::get().window;

    Game::get().consoleRenderer = std::make_unique<sf::RenderTexture>(sf::Vector2u { 256, 224 });

    lua["game"]["init"](lua["game"]);

    sf::Clock clock;
    int fps = 0;
    int frame = 0;
    Timer t(60);
    Game& game = Game::get();
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

        t.update();
        
        const int ticks = t.getTickCount();
        for (int i = 0; i < ticks; ++i) {
            game.getKVP("step").as<sol::function>()(game);
            Keys::get().update();
            if (game.room) {
                game.room->update();
            }
            if (Keys::get().pressed(sf::Keyboard::Scancode::F5)) {
                const sf::Texture& t = Game::get().consoleRenderer.get()->getTexture();
                sf::Image i = t.copyToImage();
                bool saved = i.saveToFile("_.png");
            }
        }

        Game::get().currentRenderer = Game::get().consoleRenderer.get();
        Game::get().consoleRenderer->clear();
        if (game.room) {
            float alpha = t.getAlpha();
            game.room->draw(alpha);
        }
        Game::get().consoleRenderer->display();

        window->clear();

        const auto dispSize = window->getSize();
        sf::View view(sf::FloatRect{ { 0, 0 }, { (float)dispSize.x, (float)dispSize.y } });
        view.setCenter({ dispSize.x / 2.0f, dispSize.y / 2.0f });
        window->setView(view);

        sf::Sprite renderSprite(Game::get().consoleRenderer->getTexture());
        sf::Vector2u gameSize = Game::get().consoleRenderer->getSize();

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
        Game::get().fps = 1.f / delta;

        ++frame;
    }

    ObjectManager::get().baseClasses.clear();
    TilesetManager::get().tilesets.clear();
    SpriteManager::get().sprites.clear();
}