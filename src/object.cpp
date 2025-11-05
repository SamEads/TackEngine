#include <iostream>
#include <fstream>
#include "vendor/json.hpp"
#include "object.h"
#include "game.h"
#include "room.h"

using namespace nlohmann;

std::vector<sf::Vector2f> Object::getPoints() const {
    auto it = kvp.find("points");
    if (it != kvp.end()) {
        sol::table points = it->second.as<sol::table>();

        std::vector<sf::Vector2f> v;
        v.reserve(points.size());

        for (auto& point : points) {
            auto t = point.second.as<sol::table>();
            float px = t.get<float>("x");
            float py = t.get<float>("y");
            v.push_back({ px + x, py + y });
        }
        return v;
    }

    if (imageAngle == 0) {
        sf::FloatRect rect = getRectangle();
        return std::vector<sf::Vector2f> {
            { rect.position.x, rect.position.y },
            { rect.position.x + rect.size.x, rect.position.y },
            { rect.position.x + rect.size.x, rect.position.y + rect.size.y },
            { rect.position.x, rect.position.y + rect.size.y }
        };
    }

    sf::FloatRect hb = {};
    if (maskIndex) {
        hb = maskIndex->hitbox;
    }
    else if (spriteIndex) {
        hb = spriteIndex->hitbox;
    }
    sf::Vector2f unscaledCorners[4] = {
        { hb.position.x, hb.position.y }, // top-left
        { hb.position.x + hb.size.x, hb.position.y }, // top-right
        { hb.position.x + hb.size.x, hb.position.y + hb.size.y }, // bottom-right
        { hb.position.x, hb.position.y + hb.size.y } // bottom-left
    };

    std::vector<sf::Vector2f> transformed;
    transformed.reserve(4);

    float rad = Deg2Rad(imageAngle);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    for (auto& corner : unscaledCorners) {
        sf::Vector2f scaled = { corner.x * xScale, corner.y * yScale };

        float originX = (spriteIndex) ? spriteIndex->originX : 0;
        float originY = (spriteIndex) ? spriteIndex->originY : 0;
        scaled.x -= originX * xScale;
        scaled.y -= originY * yScale;

        sf::Vector2f rotated = {
            scaled.x * cosA - scaled.y * sinA,
            scaled.x * sinA + scaled.y * cosA
        };

        rotated.x += x;
        rotated.y += y;

        transformed.push_back(rotated);
    }

    return transformed;
}

void Object::setDyn(const std::string &key, sol::main_object obj) {
    auto it = kvp.find(key);
    if (it == kvp.end()) {
        kvp.insert({ key, sol::object(std::move(obj)) });
    }
    else {
        it->second = sol::object(std::move(obj));
    }
}

sol::object Object::getDyn(const std::string &ref) {
    auto it = kvp.find(ref);
    if (it == kvp.end()) {
        return sol::lua_nil;
    }
    return it->second;
}

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
    if (!spriteIndex) return;
	float interpX = lerp(xPrev, x, alpha);
	float interpY = lerp(yPrev, y, alpha);
	spriteIndex->draw(*Game::get().currentRenderer, { interpX, interpY }, imageIndex, { xScale, yScale }, sf::Color::White, imageAngle);
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
        for (auto& prop : extends->rawProperties) {
            newObject->rawProperties[prop.first] = { prop.second.first, prop.second.second };
        }
        for (std::pair p : extends->kvp) {
            newObject->kvp.insert(p);
        }
    }

    lua.create_named_table(identifier);
    lua[identifier] = std::move(newObject);
    std::unique_ptr<BaseObject>& memPtr = lua[identifier].get<std::unique_ptr<BaseObject>&>();

    baseClasses[identifier] = ObjectManager::ScriptedInfo {
        &memPtr,
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
    const std::unordered_map<std::string, std::filesystem::path>& objectScriptPaths) {
    // potentially a parent class- ignore
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

    auto deduceType = [](const json& v) {
        if (v.is_number_integer())  return ConvertType::INTEGER;
        if (v.is_number())          return ConvertType::REAL;
        if (v.is_boolean())         return ConvertType::BOOLEAN;
        if (v.is_string())          return ConvertType::STRING;
        return ConvertType::STRING;
    };

    if (!j["parent"].is_null()) {
        std::string parentIdentifier = j["parent"];
        auto it = objMgr.baseClasses.find(parentIdentifier);
        if (it == objMgr.baseClasses.end()) {
            ObjectCreateRecursive(parentIdentifier, lua, assets, objectScriptPaths);
        }
        std::unique_ptr<BaseObject>& parentObj = *objMgr.baseClasses.at(parentIdentifier).objectPtr;
        objSol = ObjectCreate(identifier, parentObj, lua, assets, objectScriptPaths);
        std::unique_ptr<BaseObject>& obj = objSol.as<std::unique_ptr<BaseObject>&>();
        for (auto& prop : j["properties"]) {
            obj->rawProperties[prop["name"]] = { static_cast<ConvertType>(prop.value("type", static_cast<int>(deduceType(prop["value"])))), prop["value"] };
        }
    }
    else {
        objSol = ObjectCreate(identifier, nullptr, lua, assets, objectScriptPaths);
        std::unique_ptr<BaseObject>& obj = objSol.as<std::unique_ptr<BaseObject>&>();
        for (auto& prop : j["properties"]) {
            obj->rawProperties[prop["name"]] = { static_cast<ConvertType>(prop.value("type", static_cast<int>(deduceType(prop["value"])))), prop["value"] };
        }
    }

    std::unique_ptr<BaseObject>& obj = objSol.as<std::unique_ptr<BaseObject>&>();
    obj->visible = j["visible"];

    if (!j["sprite"].is_null()) {
        obj->spriteIndex = &SpriteManager::get().sprites.at(j["sprite"]);
    }

    return objSol;
}

std::unique_ptr<Object> ObjectManager::make(sol::state &lua, BaseObject *baseObject) {
    if (baseObject != NULL) {
        auto& list = baseClasses;

        auto it = std::find_if(list.begin(), list.end(), [&baseObject](const std::pair<std::string, ScriptedInfo>& p) {
            return p.second.objectPtr->get() == baseObject;
        });

        if (it != list.end()) {
            auto copied = it->second.create(baseObject);

            std::deque<BaseObject*> parents;
            BaseObject* f = baseObject->parent;
            if (f) {
                while (true) {
                    if (f) {
                        parents.push_back(f);
                        f = f->parent;
                    }
                    else break;
                }
                while (!parents.empty()) {
                    BaseObject* p = parents.front();
                    parents.pop_front();
                    for (auto& v : p->kvp) {
                        copied->setDyn(v.first, v.second);
                    }
                    if (p->rawProperties.size() > 0) {
                        sol::table t;
                        if (copied->kvp.find("properties") != copied->kvp.end()) {
                            t = copied->getDyn("properties");
                        } else {
                            t = copied->kvp.insert({ "properties", sol::table(lua, sol::create) }).first->second;
                        }
                        for (auto& [k, v] : p->rawProperties) {
                            t[k] = FieldCreateFromProperty(k, v.first, v.second, lua);
                        }
                    }
                }
            }
            for (auto& v : baseObject->kvp) {
                copied->setDyn(v.first, v.second);
            }
            if (baseObject->rawProperties.size() > 0) {
                sol::table t;
                if (copied->kvp.find("properties") != copied->kvp.end()) {
                    t = copied->getDyn("properties");
                } else {
                    t = copied->kvp.insert({ "properties", sol::table(lua, sol::create) }).first->second;
                }
                for (auto& [k, v] : baseObject->rawProperties) {
                    t[k] = FieldCreateFromProperty(k, v.first, v.second, lua);
                }
            }
            return copied;
        }
    }

    // basic object, original didn't exist
    auto copied = std::make_unique<Object>(lua);
    return copied;
}

/*
sol::table ObjectCreate(ObjectManager& objMgr, const std::string& identifier, sol::table extends, sol::state& lua) {
    std::cout << "Object create: " << identifier << "\n";

    sol::table obj = lua.create_named_table(identifier);

    if (extends == sol::lua_nil) {
        // Base object
        obj["x"] = 0;
        obj["y"] = 0;
        obj["xprevious"] = 0;
        obj["yprevious"] = 0;
        obj["hspeed"] = 0;
        obj["vspeed"] = 0;
        obj["image_xscale"] = 1;
        obj["image_yscale"] = 1;
        obj["image_speed"] = 1;
        obj["image_index"] = 0;
        obj["increment_image_speed"] = true;
        obj["object_index"] = obj;
        obj["super"] = sol::lua_nil;
        obj["id"] = 0;

        sol::table metatable = lua.create_table();
        metatable["__index"] = obj;

        obj[sol::metatable_key] = metatable;
    } else {
        sol::table metatable = lua.create_table();
        metatable["__index"] = extends;

        obj[sol::metatable_key] = metatable;
        
        obj["super"] = extends;
        obj["object_index"] = obj;
    }

    return obj;
}
*/

sol::table ObjectCreate(ObjectManager& objMgr, const std::string& identifier, sol::table extends, sol::state& lua, bool verbose = false) {
    sol::table table = sol::lua_nil;
    if (extends != sol::lua_nil) {
        table = lua.create_table();
        if (verbose) {
            std::cout << "^^^ VVV\n";
        }
        for (auto& [k, v] : extends.pairs()) {
            if (verbose) {
                std::cout << k.as<std::string>() << "\n";
            }
            table[k] = v;
        }
        if (verbose) std::cout << "--\n";
    }
    else {
        table = objMgr.createObjectTable();
    }

    table["object_index"] = lua[identifier];
    table["super"] = extends;
    table["__index"] = table;
    table["super"] = extends;
    table["name"] = identifier;

    lua[identifier] = table;

    return table;
}

sol::table recurse(
    ObjectManager& objMgr,
    sol::state& lua,
    const std::string& identifier,
    const std::filesystem::path &assets,
    const std::unordered_map<std::string, std::filesystem::path>& scripts
) {
    if (objMgr.baseClasses.find(identifier) != objMgr.baseClasses.end()) {
        return sol::lua_nil;
    }
    
    std::filesystem::path p = assets / "managed" / "objects" / (identifier + ".json");
    if (!std::filesystem::exists(p)) {
        return sol::lua_nil;
    }

    std::ifstream i(p);
    json j = json::parse(i);

    sol::table o = sol::lua_nil;

    if (!j["parent"].is_null()) {
        std::string parentStr = j["parent"];
        std::cout << "PARENT TO " << identifier << ": " << parentStr << "\n";
        if (objMgr.baseClasses.find(parentStr) == objMgr.baseClasses.end()) {
            sol::table parentObject = recurse(objMgr, lua, parentStr, assets, scripts);
            o = ObjectCreate(objMgr, identifier, parentObject, lua, parentStr == "obj_block_parent" || parentStr == "obj_qblock");
        }
        else {
            sol::table parentObject = lua[parentStr];
            o = ObjectCreate(objMgr, identifier, parentObject, lua, parentStr == "obj_block_parent" || parentStr == "obj_qblock");
        }
    }
    else {
        o = ObjectCreate(objMgr, identifier, sol::lua_nil, lua);
    }


    objMgr.baseClasses.insert({ identifier, {} });

    if (!j["sprite"].is_null()) {
        o["sprite_index"] = &SpriteManager::get().sprites.at(j["sprite"]);
    }

    // Run object file script
    auto it = scripts.find(identifier);
    if (it != scripts.end()) {
        auto res = lua.safe_script_file(it->second.string());
        if (!res.valid()) {
            sol::error e = res;
            std::cout << e.what() << "\n";
        }
    }

    return o;
}

void ObjectManager::initializeLua(sol::state &lua, const std::filesystem::path &assets) {
    for (auto& it : std::filesystem::recursive_directory_iterator(assets / "scripts")) {
        if (it.is_regular_file()) {
            if (it.path().extension() == ".lua") {
                std::string identifier = it.path().filename().replace_extension("").string();
                scriptPaths[identifier] = it.path();
            }
        }
    }

    createObjectTable = lua.load(
R"(
local table = {
    x = 0,
    y = 0,
    xprevious = 0,
    yprevious = 0,
    hspeed = 0,
    vspeed = 0,
    image_xscale = 1,
    image_yscale = 1,
    image_speed = 1,
    sprite_index = nil,
    image_index = 0,
    increment_image_speed = true,
    image_angle = 0
}
function table:bbox_left()
    return 0
end

function table:bbox_right()
    return 16
end

function table:bbox_top()
    return 0
end

function table:bbox_bottom()
    return 16
end

return table
)");

    lua["object_create"] = [&](const std::string& identifier, sol::table extends) {
        return ObjectCreate(*this, identifier, extends, lua);
    };

    for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "objects")) {
        if (!it.is_regular_file()) {
            continue;
        }
        std::string identifier = it.path().filename().replace_extension("").string(); // okay then
        
        recurse(*this, lua, identifier, assets, scriptPaths);

        // auto table = lua.create_named_table(identifier);
    }

    /*
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

        "depth",        &Object::depth,
        "visible",      &Object::visible,
        "active",       &Object::active,

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
        "bbox_left", [](const Object::Reference& caller) { return caller.object->bboxLeft(); },
        "bbox_right", [](const Object::Reference& caller) { return caller.object->bboxRight(); },
        "bbox_top", [](const Object::Reference& caller) { return caller.object->bboxTop(); },
        "bbox_bottom", [](const Object::Reference& caller) { return caller.object->bboxBottom(); },
        "force_position", [](const Object::Reference& caller, float x, float y) {
            caller.object->forcePosition(x, y);
        },
        "force_x", [](const Object::Reference& caller, float x) {
            caller.object->x = x;
            caller.object->xPrev = x;
        },
        "force_y", [](const Object::Reference& caller, float y) {
            caller.object->y = y;
            caller.object->yPrev = y;
        },
        "extends", [](const Object::Reference& caller, BaseObject* base) {
            return caller.object->extends(base);
        },
        sol::meta_function::equal_to, [](const Object::Reference& a, const Object::Reference& b) {
            return (a.object == b.object);
        },
        sol::meta_function::index, [](Object::Reference& self, const std::string& key, sol::this_state s) -> sol::object {
            return self.table[key];
        },
        sol::meta_function::new_index, [](Object::Reference& self, const std::string& key, sol::object value) {
            self.table[key] = value;
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
    */

}