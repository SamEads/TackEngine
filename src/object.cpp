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

const bool Object::extends(BaseObject *o) const
{
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

    auto deduceType = [](const json& v) {
        if (v.is_number_integer()) {
            return ConvertType::INTEGER;
        }
        else if (v.is_number()) {
            return ConvertType::REAL;
        }
        else if (v.is_boolean()) {
            return ConvertType::BOOLEAN;
        }
        else if (v.is_string()) {
            return ConvertType::STRING;
        }
        return ConvertType::STRING;
    };

    if (!j["parent"].is_null()) {
        std::string parentIdentifier = j["parent"];
        auto it = objMgr.baseClasses.find(parentIdentifier);
        if (it == objMgr.baseClasses.end()) {
            ObjectCreateRecursive(parentIdentifier, lua, assets, objectScriptPaths);
        }
        std::unique_ptr<BaseObject>& parentObj = objMgr.baseClasses.at(parentIdentifier).object.as<std::unique_ptr<BaseObject>&>();
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
    std::unique_ptr<Object>& obj = objSol.as<std::unique_ptr<Object>&>();

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
            return p.second.object.as<BaseObject*>() == baseObject;
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
        "bbox_width",     sol::readonly_property(&Object::bboxWidth),
        "bbox_height",     sol::readonly_property(&Object::bboxHeight),
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