#include <iostream>
#include <fstream>
#include "vendor/json.hpp"
#include "object.h"
#include "game.h"
#include "room.h"

using namespace nlohmann;

const bool Object::extends(BaseObject *o) const {
    if (o == nullptr) return false;
    if (self == o) return true;
    if (parent == nullptr) return false;

    BaseObject* check = parent;
    while (true) {
        // found match
        if (check == o) {
            return true;
        }

        // continue upwards list search
        if (check->parent != nullptr) {
            check = check->parent;
        }
        else {
            break;
        }
    }
    return false;
}

void Object::draw(Room *room, float alpha) {
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
    const std::unique_ptr<BaseObject>& extends,
    sol::state& lua, const std::filesystem::path& assets,
    const std::unordered_map<std::string, std::filesystem::path>& objectScriptPaths)
{
    auto& objMgr = ObjectManager::get();
    auto& baseClasses = objMgr.baseClasses;

    if (baseClasses.find(identifier) != baseClasses.end()) {
        return sol::object(sol::lua_nil);
    }

    std::unique_ptr<BaseObject> newObject = std::make_unique<BaseObject>(lua);

    if (extends != nullptr) {
        newObject->parent = extends.get();
        for (std::pair p : extends->kvp) {
            newObject->kvp.insert(p);
        }
    }

    lua.create_named_table(identifier);
    lua[identifier] = std::move(newObject);
    std::unique_ptr<BaseObject>& memPtr = lua[identifier].get<std::unique_ptr<BaseObject>&>();

    baseClasses[identifier] = ObjectManager::ScriptedInfo {
        lua[identifier],
        [](BaseObject* original) {
            return std::make_unique<Object>(*original);
        }
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

    std::filesystem::path p = assets / "managed" / "objects" / (identifier + ".json");
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
        std::unique_ptr<BaseObject>& parentObj = objMgr.baseClasses.at(parentIdentifier).object.as<std::unique_ptr<BaseObject>&>();
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
        "object_index", sol::readonly(&Object::self),
        "bbox_left",    sol::readonly_property(&Object::bboxLeft),
        "bbox_right",   sol::readonly_property(&Object::bboxRight),
        "bbox_bottom",  sol::readonly_property(&Object::bboxBottom),
        "bbox_top",     sol::readonly_property(&Object::bboxTop),
        "bbox_top",     sol::readonly_property(&Object::bboxTop),
        "depth",        &Object::depth,
        "visible",      &Object::visible,
        sol::meta_function::index,      &Object::getDyn,
        sol::meta_function::new_index,  &Object::setDyn
    );

    lua.new_usertype<BaseObject>(
        "BaseObject",       sol::no_constructor,
        sol::meta_function::index,      &BaseObject::getDyn,
        sol::meta_function::new_index,  &BaseObject::setDyn,
        sol::base_classes, sol::bases<Object>()
    );

    lua.new_usertype<Object::Reference>(
        "ObjectReference", sol::no_constructor,
        "extends", [](const Object::Reference& caller, BaseObject* base) {
            return caller.object.as<Object*>()->extends(base);
        },
        sol::meta_function::equal_to, [](const Object::Reference& a, const Object::Reference& b) {
            return (a.object == b.object);
        },
        sol::meta_function::index, [](Object::Reference& self, const std::string& key, sol::this_state s) -> sol::object {
            sol::state_view lua(s);
            if (self.object) {
                return self.object.as<sol::table>()[key];
            }
            else {
                return sol::make_object(lua, sol::lua_nil);
            }
        },
        sol::meta_function::new_index, [](Object::Reference& self, const std::string& key, sol::object value) {
            self.object.as<sol::table>()[key] = value;
        }
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
        if (extends.is<std::unique_ptr<BaseObject>>()) {
            return ObjectCreate(identifier, extends.as<std::unique_ptr<BaseObject>&>(), lua, assets, scriptPaths);
        }
        else {
            return ObjectCreate(identifier, nullptr, lua, assets, scriptPaths);
        }
    };

    for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "objects")) {
        if (!it.is_regular_file()) {
            continue;
        }
        std::string identifier = it.path().filename().replace_extension("").string(); // okay then
        ObjectCreateRecursive(identifier, lua, assets, scriptPaths);
    }
}