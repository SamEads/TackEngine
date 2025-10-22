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
#include "utility/timer.h"
#include "shader.h"
#include "sound.h"
#include "font.h"

using namespace nlohmann;

void InitializeLuaEnvironment(sol::state& lua) {
    std::filesystem::path assets = Game::get().assetsFolder;

    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::jit);
    std::filesystem::path scriptsPath = (std::filesystem::path(assets.string()) / "scripts" / "?.lua");
    lua["package"]["path"] = scriptsPath.string();

    lua["math"]["sign"] = [](float number) {
        if (number > 0) return 1;
        if (number < 0) return -1;
        return 0;
    };

    lua["math"]["point_distance"] = [](float x1, float y1, float x2, float y2) {
        return PointDistance(x1, y1, x2, y2);
    };

    lua["math"]["lerp"] = [](float a, float b, float t) {
        return lerp(a, b, t);
    };

    lua["math"]["intersects"] = [](sol::table a, sol::table b) {
        sf::FloatRect fa = { { a.get<float>(1), a.get<float>(2) }, { a.get<float>(3), a.get<float>(4) } };
        sf::FloatRect fb = { { b.get<float>(1), b.get<float>(2) }, { b.get<float>(3), b.get<float>(4) } };
        return fa.findIntersection(fb).has_value();
    };

    lua.new_usertype<Tilemap>(
        "Tilemap", sol::no_constructor,
        "visible", &Tilemap::visible,
        "depth", &Tilemap::depth
    );

    lua.new_usertype<Background>(
        "Background", sol::no_constructor,
        "visible", &Background::visible,
        "depth", &Background::depth,
        "sprite_index", &Background::spriteIndex
    );

    lua.new_usertype<Room>(
        "Room", sol::no_constructor,

        // Room info
        "width", sol::readonly(&Room::width),
        "height", sol::readonly(&Room::height),
        "camera_x", sol::property(&Room::getCameraX, &Room::setCameraX),
        "camera_xprevious", sol::readonly(&Room::cameraPrevX),
        "camera_yprevious", sol::readonly(&Room::cameraPrevY),
        "camera_y", sol::property(&Room::getCameraY, &Room::setCameraY),
        "camera_width", sol::readonly(&Room::cameraWidth),
        "camera_height", sol::readonly(&Room::cameraHeight),

        // Objects & instances
        "instance_create", &Room::instanceCreateScript,
        "instance_exists", &Room::instanceExists,
        "instance_destroy", &Room::instanceDestroyScript,
        "object_destroy", &Room::objectDestroy,
        "object_exists", &Room::objectExists,
        "object_count", &Room::objectCount,
        "object_get", &Room::getObject,

        // Layers
        "tile_layer_get", &Room::getTileLayer,
        "background_layer_get", &Room::getBackgroundLayer,

        // Collisions
        "instance_place", &Room::instancePlaceScript,
        "collision_rectangle", &Room::collisionRectangleScript,
        "collision_rectangle_list", &Room::collisionRectangleListScript,

        sol::meta_function::index,      &Room::getKVP,
        sol::meta_function::new_index,  &Room::setKVP
    );

    for (auto& it : std::filesystem::directory_iterator(assets / "rooms")) {
        if (!it.is_directory()) {
            continue;
        }

        std::filesystem::path p = it.path();
        std::string identifier = p.filename().string();

        RoomReference& ref = Game::get().rooms[identifier];
        ref.name = identifier;
        ref.p = p;

        lua[identifier] = ref;
    }

    TilesetManager::get().initializeLua(lua, assets);
    SpriteManager::get().initializeLua(lua, assets);
    ObjectManager::get().initializeLua(lua, assets);
    FontManager::get().initializeLua(lua, assets);
    SoundManager::get().initializeLua(lua, assets);
    MusicManager::get().initializeLua(lua);
    ShaderManager::get().initializeLua(lua);
    Keys::get().initializeLua(lua);
    Game::get().initializeLua(lua, assets);
}

#define GMC_EMBEDDED
#define GMCONVERT_IMPLEMENTATION
#include "gmconvert.h"

int main() {
    std::thread gameThread = std::thread([&] {
        sol::state& lua = Game::get().lua;

        auto res = lua.safe_script_file("assets/gmconvert.lua");
        if (!res.valid()) {
            sol::error e = res;
            std::cout << e.what() << "\n";
        }

        std::filesystem::path p = std::filesystem::path(lua["GM_project_directory"].get<std::string>());
        GMConvert(p, "assets");

        InitializeLuaEnvironment(lua);

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

            float scaleX = floorf(dispSize.x / (float)gameSize.x);
            float scaleY = floorf(dispSize.y / (float)gameSize.y);
            float scale = std::max(1.0f, std::min(scaleX, scaleY));

            renderSprite.setScale({ scale, scale });

            float offsetX = floorf((dispSize.x - (gameSize.x * scale)) / 2.f);
            float offsetY = floorf((dispSize.y - (gameSize.y * scale)) / 2.f);
            renderSprite.setPosition({ offsetX, offsetY });

            window->draw(renderSprite);

            window->display();

            float delta = clock.restart().asSeconds();
            Game::get().fps = 1.f / delta;

            ++frame;

        }

        ObjectManager::get().baseClasses.clear();
        TilesetManager::get().tilesets.clear();
        SpriteManager::get().sprites.clear();
    });

    while (true) {
        std::string s;
        std::cin >> s;
        auto res = Game::get().lua.safe_script(s);
        if (!res.valid()) {
            sol::error e = res;
            std::cout << e.what() << "\n";
        }
    }

    gameThread.join();
}