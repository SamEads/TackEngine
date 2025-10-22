#include <iostream>
#include <fstream>
#include "vendor/json.hpp"
#include "object.h"
#include "game.h"
#include "room.h"

using namespace nlohmann;

void Object::draw(Room* room, float alpha) {
	if (runScript("draw", room, alpha) || !spriteIndex) {
		return;
	}
	float interpX = lerp(xPrev, x, alpha);
	float interpY = lerp(yPrev, y, alpha);
	spriteIndex->draw(*Game::get().currentRenderer, { floorf(interpX), floorf(interpY) }, imageIndex, { xScale, yScale }, sf::Color::White, imageAngle);
}

void Object::beginDraw(Room *room, float alpha) {
	runScript("begin_draw", room, alpha);
}

void Object::endDraw(Room *room, float alpha) {
	runScript("end_draw", room, alpha);
}

void Object::drawGui(Room *room, float alpha) {
	runScript("draw_gui", room, alpha);
}

static sol::object ObjectCreate(
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

static sol::object ObjectCreateRecursive(
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
            ObjectCreateRecursive(parentIdentifier, lua, assets, objectScriptPaths);
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

void ObjectManager::initializeLua(sol::state &lua, const std::filesystem::path &assets) {
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
        "increment_image_speed",  &Object::incrementImageSpeed,
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

    for (auto& it : std::filesystem::recursive_directory_iterator(assets / "scripts")) {
        if (it.is_regular_file()) {
            if (it.path().extension() == ".lua") {
                std::string identifier = it.path().filename().replace_extension("").string();
                scriptPaths[identifier] = it.path();
            }
        }
    }

    lua["object_create"] = [&](const std::string& identifier, sol::object extends) {
        if (extends.is<std::unique_ptr<Object>>()) {
            return ObjectCreate(identifier, extends.as<std::unique_ptr<Object>&>(), lua, assets, scriptPaths);
        }
        return ObjectCreate(identifier, nullptr, lua, assets, scriptPaths);
    };

    for (auto& it : std::filesystem::directory_iterator(assets / "objects")) {
        if (!it.is_regular_file()) {
            continue;
        }
        std::string identifier = it.path().filename().replace_extension("").string(); // okay then
        ObjectCreateRecursive(identifier, lua, assets, scriptPaths);
    }
}