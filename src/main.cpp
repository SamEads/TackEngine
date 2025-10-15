#include <fstream>
#include "vendor/json.hpp"
#include "sprite.h"
#include "object.h"
#include "tileset.h"
#include "game.h"
#include "room.h"
#include "keys.h"
#include "sound.h"

using namespace nlohmann;

sol::object ObjectCreate(const std::string& identifier, const std::unique_ptr<Object>& extends, sol::state& lua) {
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
    return solObject;
}

static sol::object ObjectCreateRec(std::string identifier, sol::state& lua, const std::filesystem::path& assets) {
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
        auto it = objMgr.baseClasses.find(j["parent"]);
        if (it == objMgr.baseClasses.end()) {
            ObjectCreateRec(j["parent"], lua, assets);
        }
        auto& parentObj = objMgr.baseClasses.at(j["parent"]).object.as<std::unique_ptr<Object>&>();
        objSol = ObjectCreate(identifier, parentObj, lua);
    }
    else {
        objSol = ObjectCreate(identifier, nullptr, lua);
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

    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table);
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
        "get_id",      &Object::makeReference,
        "depth",        &Object::depth,
        "visible",      &Object::visible,
        "extends",      &Object::extends,
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
        "visible", &Tilemap::visible
    );

    lua.new_usertype<Background>(
        "Background", sol::no_constructor,
        "visible", &Background::visible
    );

    lua.new_usertype<Room>(
        "Room", sol::no_constructor,
        "camera_x", sol::property(&Room::getCameraX, &Room::setCameraX),
        "camera_y", sol::property(&Room::getCameraY, &Room::setCameraY),
        "width", sol::readonly(&Room::width),
        "height", sol::readonly(&Room::height),
        "instance_create", &Room::instanceCreateScript,
        "instance_exists", &Room::instanceExists,
        "object_exists", &Room::objectExists,
        "object_get", &Room::getObject,
        "tile_layer_get", &Room::getTileLayer,
        "background_layer_get", &Room::getBackgroundLayer,
        "instance_place", &Room::instancePlaceScript,
        "collision_rectangle", &Room::collisionRectangleScript,
        "collision_rectangle_list", &Room::collisionRectangleListScript,
        "instance_destroy", &Room::instanceDestroyScript
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

        sol::object o = sol::make_object(lua.lua_state(), &spr);
        lua[identifier] = o;
    }

    lua["object_create"] = [&](const std::string& identifier, sol::object extendsMaybe) {
        if (extendsMaybe.is<std::unique_ptr<Object>>()) {
            return ObjectCreate(identifier, extendsMaybe.as<std::unique_ptr<Object>&>(), lua);

        }
        return ObjectCreate(identifier, nullptr, lua);
    };

    // Load objects
    ObjectManager& objMgr = ObjectManager::get();
    std::vector<std::string> objectsToRunLua;
    for (auto& it : std::filesystem::directory_iterator(assets / "objects")) {
        if (!it.is_regular_file()) {
            continue;
        }
        std::string identifier = it.path().filename().replace_extension("").string(); // okay then
        ObjectCreateRec(identifier, lua, assets);
        objectsToRunLua.push_back(identifier);
    }

    for (auto& str : objectsToRunLua) {
        std::filesystem::path corresponding = assets / "scripts" / "objects" / (str + ".lua");
        if (std::filesystem::exists(corresponding)) {
            auto res = lua.safe_script_file(corresponding.string());
            if (!res.valid()) {
                sol::error e = res;
                std::cout << e.what() << "\n";
            }
        }
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

    lua["play_sound"] = [&](const std::string& path) {
        SoundManager::get().play(path + ".wav");
    };

    lua["gfx"]["draw_sprite"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y) {
        if (spriteIndex == nullptr) return;
        spriteIndex->draw(*Game::get().currentRenderer, { x, y }, imageIndex);
    };

    lua["gfx"]["draw_sprite_ext"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y, float xscale, float yscale, float rot, sol::table color) {
        if (spriteIndex == nullptr) return;
        spriteIndex->draw(*Game::get().currentRenderer, { x, y }, imageIndex, { xscale, yscale }, MakeColor(color), rot);
    };
}

#define GMC_EMBEDDED
#define GMCONVERT_IMPLEMENTATION
#include "gmconvert.h"

int main() {
    sol::state lua;

    std::cout << "1\n";

    auto res = lua.safe_script_file("assets/gmconvert.lua");
    if (!res.valid()) {
        sol::error e = res;
        std::cout << e.what() << "\n";
    }
    std::cout << "2\n";

    std::filesystem::path p = std::filesystem::path(lua["GM_project_directory"].get<std::string>());
    GMConvert(p, "assets");
    std::cout << "3\n";

    InitializeLuaEnvironment(lua);
    std::cout << "4\n";

    SoundManager& sndMgr = SoundManager::get();
    sndMgr.thread = std::thread(&SoundManager::update, &sndMgr);
    Game::get().window = std::make_unique<sf::RenderWindow>(sf::VideoMode({ 256 * 2, 224 * 2 }), "TackEngine");
    auto& window = Game::get().window;
    window->setFramerateLimit(60);
    std::cout << "5\n";

    Game::get().consoleRenderer = std::make_unique<sf::RenderTexture>(sf::Vector2u { 256, 224 });

    Room r(lua, "rm_2_1_a");
    while (window->isOpen()) {
        while (const std::optional event = window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window->close();
            }
        }

        Keys::get().update();

        r.update();

        Game::get().currentRenderer = Game::get().consoleRenderer.get();
        Game::get().consoleRenderer->clear();
        r.draw();
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
    }

    ObjectManager::get().baseClasses.clear();
    TilesetManager::get().tilesets.clear();
    SpriteManager::get().sprites.clear();
}