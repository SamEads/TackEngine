extern "C" {
#include <luajit.h>
}
#include <fstream>
#include "vendor/json.hpp"
#include "sprite.h"
#include "object.h"
#include "tileset.h"
#include "game.h"
#include "room.h"
#include "keys.h"
#include "utility/timer.h"
#include "sound.h"
#include "font.h"

using namespace nlohmann;

sol::object ObjectCreate(
    const std::string& identifier,
    const std::unique_ptr<Object>& extends,
    sol::state& lua, const std::filesystem::path& assets,
    const std::unordered_map<std::string, std::filesystem::path>& objectScriptPaths)
{
    auto& objMgr = ObjectManager::get();
    auto& baseClasses = objMgr.baseClasses;

    if (baseClasses.find(identifier) != baseClasses.end()) {
        return sol::object(sol::lua_nil);
    }

    std::unique_ptr<Object> newObject = std::make_unique<Object>(lua);

    if (extends != nullptr) {
        newObject->parent = extends.get();
        for (std::pair p : extends->kvp) {
            newObject->kvp.insert(p);
        }
    }

    lua.create_named_table(identifier);
    lua[identifier] = std::move(newObject);
    std::unique_ptr<Object>& memPtr = lua[identifier].get<std::unique_ptr<Object>&>();

    baseClasses[identifier] = ObjectManager::ScriptedInfo {
        lua[identifier],
        [](std::unique_ptr<Object>& original) { return std::make_unique<Object>(*(original.get())); }
    };


    sol::object solObject = lua[identifier];
    memPtr->self = memPtr.get();
    memPtr->identifier = identifier;

    // Run script for object
    auto it = objectScriptPaths.find(identifier);
    if (it != objectScriptPaths.end()) {
        auto res = lua.safe_script_file(it->second.string());
        if (!res.valid()) {
            sol::error e = res;
            std::cout << e.what() << "\n";
        }
    }

    return solObject;
}

static sol::object ObjectCreateRec(
    std::string identifier,
    sol::state& lua,
    const std::filesystem::path& assets,
    const std::unordered_map<std::string, std::filesystem::path>& objectScriptPaths)
{
    // potentially a parent class-ignore
    ObjectManager& objMgr = ObjectManager::get();
    if (objMgr.baseClasses.find(identifier) != objMgr.baseClasses.end()) {
        return sol::object(sol::lua_nil);
    }

    std::filesystem::path p = assets / "objects" / (identifier + ".json");
    if (!std::filesystem::exists(p)) {
        return sol::object(sol::lua_nil);
    }

    std::ifstream i(p);
    json j = json::parse(i);

    sol::object objSol = sol::lua_nil;

    if (!j["parent"].is_null()) {
        std::string parentIdentifier = j["parent"];
        auto it = objMgr.baseClasses.find(parentIdentifier);
        if (it == objMgr.baseClasses.end()) {
            ObjectCreateRec(parentIdentifier, lua, assets, objectScriptPaths);
        }
        std::unique_ptr<Object>& parentObj = objMgr.baseClasses.at(parentIdentifier).object.as<std::unique_ptr<Object>&>();
        objSol = ObjectCreate(identifier, parentObj, lua, assets, objectScriptPaths);
    }
    else {
        objSol = ObjectCreate(identifier, nullptr, lua, assets, objectScriptPaths);
    }
    std::unique_ptr<Object>& obj = objSol.as<std::unique_ptr<Object>&>();

    obj->visible = j["visible"];

    if (!j["sprite"].is_null()) {
        obj->spriteIndex = &SpriteManager::get().sprites.at(j["sprite"]);
    }

    return objSol;
}

sf::Color MakeColor(sol::table c) {
    uint8_t r = c.get<int>(1);
    uint8_t g = c.get<int>(2);
    uint8_t b = c.get<int>(3);
    uint8_t a = c.get<int>(4);
    return sf::Color{ r, g, b, a };
};

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

    lua.new_usertype<Object>(
        "Object",       sol::no_constructor,
        "x",            &Object::x,
        "y",            &Object::y,
        "hspeed",       &Object::xspd,
        "vspeed",       &Object::yspd,
        "sprite_index", &Object::spriteIndex,
        "mask_index",   &Object::maskIndex,
        "image_index",  &Object::imageIndex,
        "image_speed",  &Object::imageSpeed,
        "image_angle",  &Object::imageAngle,
        "image_xscale", &Object::xScale,
        "image_yscale", &Object::yScale,
        "xprevious",    sol::readonly(&Object::xPrev),
        "yprevious",    sol::readonly(&Object::yPrev),
        "bbox_left",    sol::property(&Object::bboxLeft, &Object::trySet),
        "bbox_right",   sol::property(&Object::bboxRight, &Object::trySet),
        "bbox_bottom",  sol::property(&Object::bboxBottom, &Object::trySet),
        "bbox_top",     sol::property(&Object::bboxTop, &Object::trySet),
        "get_id",       &Object::makeReference,
        "depth",        &Object::depth,
        "visible",      &Object::visible,
        "extends",      &Object::extends,
        // "super",        &Object::parent,
        sol::meta_function::index,      &Object::getDyn,
        sol::meta_function::new_index,  &Object::setDyn
    );

    lua.new_usertype<Object::Reference>(
        "ObjectReference", sol::no_constructor,
        "object", &Object::Reference::object
    );

    lua.new_usertype<SpriteIndex>(
        "SpriteIndex", sol::no_constructor,
        "origin_x", sol::readonly(&SpriteIndex::originX),
        "origin_y", sol::readonly(&SpriteIndex::originY),
        "height", sol::readonly(&SpriteIndex::height),
        "width", sol::readonly(&SpriteIndex::width)
    );

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

    lua.new_usertype<Game>(
        "Game",         sol::no_constructor,
        "fps",          sol::readonly(&Game::fps),
        sol::meta_function::index,      &Game::getKVP,
        sol::meta_function::new_index,  &Game::setKVP
    );

    lua.new_usertype<Room>(
        "Room", sol::no_constructor,

        // Room info
        "width", sol::readonly(&Room::width),
        "height", sol::readonly(&Room::height),
        "camera_x", sol::property(&Room::getCameraX, &Room::setCameraX),
        "camera_y", sol::property(&Room::getCameraY, &Room::setCameraY),
        "camera_width", sol::readonly(&Room::cameraWidth),
        "camera_height", sol::readonly(&Room::cameraHeight),

        // Objects & instances
        "instance_create", &Room::instanceCreateScript,
        "instance_exists", &Room::instanceExists,
        "instance_destroy", &Room::instanceDestroyScript,
        "object_exists", &Room::objectExists,
        "object_get", &Room::getObject,

        // Layers
        "tile_layer_get", &Room::getTileLayer,
        "background_layer_get", &Room::getBackgroundLayer,

        // Collisions
        "instance_place", &Room::instancePlaceScript,
        "collision_rectangle", &Room::collisionRectangleScript,
        "collision_rectangle_list", &Room::collisionRectangleListScript
    );

    // Load sprites
    SpriteManager& sprMgr = SpriteManager::get();
    for (auto& it : std::filesystem::directory_iterator(assets / "sprites")) {
        if (!it.is_directory()) {
            continue;
        }

        std::filesystem::path p = it.path();
        std::string identifier = p.filename().string();

        std::ifstream i(p / "data.json");
        json j = json::parse(i);

        SpriteIndex& spr = sprMgr.sprites[identifier];

        spr.texture = sf::Texture(p / "frames.png");
        spr.sprite = std::make_unique<sf::Sprite>(spr.texture);
        spr.width = j["width"];
        spr.height = j["height"];
        spr.originX = j["origin_x"];
        spr.originY = j["origin_y"];

        spr.hitbox.position.x = j["bbox_left"];
        spr.hitbox.position.y = j["bbox_top"];
        spr.hitbox.size.x = j["bbox_right"].get<float>() - spr.hitbox.position.x + 1.0f;
        spr.hitbox.size.y = j["bbox_bottom"].get<float>() - spr.hitbox.position.y + 1.0f;

        // temp handling of frames:
        int frameCount = j["frames"];
        for (int i = 0; i < frameCount; ++i) {
            spr.frames.push_back({ i * spr.width, 0 });
        }

        lua[identifier] = &spr;
    }

    std::unordered_map<std::string, std::filesystem::path> objectScriptPaths;
    for (auto& it : std::filesystem::recursive_directory_iterator(assets / "scripts")) {
        if (it.is_regular_file()) {
            if (it.path().extension() == ".lua") {
                std::string identifier = it.path().filename().replace_extension("").string();
                objectScriptPaths[identifier] = it.path();
            }
        }
    }

    lua["object_create"] = [&](const std::string& identifier, sol::object extendsMaybe) {
        if (extendsMaybe.is<std::unique_ptr<Object>>()) {
            return ObjectCreate(identifier, extendsMaybe.as<std::unique_ptr<Object>&>(), lua, assets, objectScriptPaths);

        }
        return ObjectCreate(identifier, nullptr, lua, assets, objectScriptPaths);
    };

    // Load objects
    ObjectManager& objMgr = ObjectManager::get();

    for (auto& it : std::filesystem::directory_iterator(assets / "objects")) {
        if (!it.is_regular_file()) {
            continue;
        }
        std::string identifier = it.path().filename().replace_extension("").string(); // okay then
        ObjectCreateRec(identifier, lua, assets, objectScriptPaths);
    }

    // Load tilesets
    TilesetManager& tsMgr = TilesetManager::get();
    for (auto& it : std::filesystem::directory_iterator(assets / "tilesets")) {
        if (!it.is_regular_file()) {
            continue;
        }
        std::string identifier = it.path().filename().replace_extension("").string();
        std::ifstream i(it.path());
        json j = json::parse(i);

        Tileset ts = {};

        ts.offsetX = j["offset_x"];
        ts.offsetY = j["offset_y"];
        ts.separationX = j["separation_x"];
        ts.separationY = j["separation_y"];
        ts.spriteIndex = &sprMgr.sprites[j["sprite"]];
        ts.tileCount = j["tile_count"];
        ts.tileWidth = j["tile_width"];
        ts.tileHeight = j["tile_height"];
        ts.tileCountX = ts.spriteIndex->width / ts.tileWidth;
        ts.tileCountY = ts.spriteIndex->height / ts.tileHeight;

        tsMgr.tilesets[identifier] = ts;
    }

    Keys::InitializeLuaEnums(lua);
    lua["keyboard_check"] = [](sf::Keyboard::Scancode key) {
        return Keys::get().held(key);
    };
    lua["keyboard_pressed"] = [](sf::Keyboard::Scancode key) {
        return Keys::get().pressed(key);
    };
    lua["keyboard_released"] = [](sf::Keyboard::Scancode key) {
        return Keys::get().released(key);
    };

    // GFX
    lua.create_named_table("gfx");

    lua["gfx"]["draw_rectangle"] = [&](float x1, float y1, float x2, float y2, sol::table color) {
        sf::RectangleShape rs({ x2 - x1, y2 - y1 });
        rs.setPosition({ x1, y1 });
        rs.setFillColor(MakeColor(color));
        Game::get().currentRenderer->draw(rs);
    };

    lua["gfx"]["draw_sprite"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y) {
        if (spriteIndex == nullptr) return;
        spriteIndex->draw(*Game::get().currentRenderer, { x, y }, imageIndex);
    };

    lua["gfx"]["draw_sprite_ext"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y, float xscale, float yscale, float rot, sol::table color) {
        if (spriteIndex == nullptr) return;
        spriteIndex->draw(*Game::get().currentRenderer, { x, y }, imageIndex, { xscale, yscale }, MakeColor(color), rot);
    };

    lua["gfx"]["draw_sprite_ext_origin"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y, float xscale, float yscale, float originX, float originY, float rot, sol::table color) {
        if (spriteIndex == nullptr) return;
        spriteIndex->drawOrigin(*Game::get().currentRenderer, { x - spriteIndex->originX + originX, y - spriteIndex->originY + originY }, imageIndex, { xscale, yscale }, { originX, originY }, MakeColor(color), rot);
    };

    lua.create_named_table("font");
    lua["font"]["add"] = [&](const std::string fontName, SpriteIndex* spriteIndex, const std::string& order) {
        auto& fm = FontManager::get();
        auto& font = fm.fonts[fontName];
        font.spriteIndex = spriteIndex;
        for (int i = 0; i < order.length(); ++i) {
            font.charMap[order[i]] = i;
        }
        lua[fontName] = &fm.fonts[fontName];
    };

    lua["font"]["draw"] = [&](float x, float y, Font* font, int spacing, const std::string& string, sol::table color) {
        auto& spriteIndex = font->spriteIndex;
        sf::RenderTarget& currentRenderer = *Game::get().currentRenderer;

        float cx = 0, cy = 0;
        for (int i = 0; i < string.length(); ++i) {
            char c = string[i];
            if (c == '\n') {
                cx = 0;
                cy += spriteIndex->height;
                continue;
            }
            if (c != ' ') {
                spriteIndex->draw(currentRenderer, { x + cx, y + cy }, font->charMap[string[i]], { 1, 1 }, MakeColor(color));
            }
            cx += spriteIndex->width + spacing;
        }
    };

    lua.new_usertype<ScriptSound>(
        "Sound", sol::no_constructor
    );

    // Sounds
    for (auto& it : std::filesystem::directory_iterator(assets / "sounds")) {
        if (!it.is_directory()) continue;

        std::string soundName = it.path().filename().string();

        auto data = it.path() / "data.json";
        std::ifstream i(data);
        json j = json::parse(i);
        std::string& extension = j["extension"].get<std::string>();

        ScriptSound scriptSound;
        scriptSound.file = std::filesystem::path(soundName) / std::string("sound" + extension);
        scriptSound.volume = j["volume"];

        lua[soundName] = scriptSound;
    }

    lua.create_named_table("sound");
    lua["sound"]["is_playing"] = [&](const ScriptSound& sound) {
        return SoundManager::get().isPlaying(sound.file.string());
    };
    lua["sound"]["play"] = [&](const ScriptSound& sound, float gain, float pitch) {
        SoundManager::get().play(sound.file.string(), pitch, sound.volume * gain);
    };
    lua["sound"]["stop"] = [&](const ScriptSound& sound) {
        SoundManager::get().stop(sound.file.string());
    };

    lua["window_set_caption"] = [&](const std::string& caption) {
        Game::get().window->setTitle(caption);
    };

    /*
    lua["window_get_caption"] = [&]() {
        Game::get().windowTitle;
    };
    */

    lua["game"] = &Game::get();
    auto gameRes = lua.safe_script_file(std::filesystem::path(assets / "scripts" / "game.lua").string());
    if (!gameRes.valid()) {
        sol::error e = gameRes;
        std::cout << e.what() << "\n";
    }
}

#define GMC_EMBEDDED
#define GMCONVERT_IMPLEMENTATION
#include "gmconvert.h"

int main() {
    sol::state lua;

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
    Game::get().window = std::make_unique<sf::RenderWindow>(sf::VideoMode({ 256 * 2, 224 * 2 }), "TackEngine");
    auto& window = Game::get().window;

    Game::get().consoleRenderer = std::make_unique<sf::RenderTexture>(sf::Vector2u { 256, 224 });

    lua["game"]["init"](lua["game"]);

    Room r(lua, "rm_2_1_a");

    sf::Clock clock;
    int fps = 0;
    int frame = 0;
    Timer t(60);
    while (window->isOpen()) {
        while (const std::optional event = window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window->close();
            }
        }

        t.update();
        
        const int ticks = t.getTickCount();
        for (int i = 0; i < ticks; ++i) {
            Keys::get().update();
            r.update();
        }

        float alpha = t.getAlpha();
        Game::get().currentRenderer = Game::get().consoleRenderer.get();
        Game::get().consoleRenderer->clear();
        r.draw(alpha);
        Game::get().consoleRenderer->display();

        window->clear();

        const auto dispSize = window->getSize();
        sf::View view(sf::FloatRect{ { 0, 0 }, { (float)dispSize.x, (float)dispSize.y } });
        view.setCenter({ dispSize.x / 2.0f, dispSize.y / 2.0f });
        window->setView(view);

        sf::Sprite renderSprite(Game::get().consoleRenderer->getTexture());

        float scaleX = floorf(dispSize.x / (float)256);
        float scaleY = floorf(dispSize.y / (float)224);
        float scale = std::max(1.0f, std::min(scaleX, scaleY));

        renderSprite.setScale({ scale, scale });

        float offsetX = floorf((dispSize.x - (256 * scale)) / 2.f);
        float offsetY = floorf((dispSize.y - (224 * scale)) / 2.f);
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
}