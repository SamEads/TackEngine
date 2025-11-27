#include <iostream>
#include <fstream>
#include "vendor/json.hpp"
#include "object.h"
#include "game.h"
#include "room.h"

using namespace nlohmann;

std::vector<sf::Vector2f> Object::getPoints() const {
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
    
	float interpX = lerp(xPrevRender, x, alpha);
	float interpY = lerp(yPrevRender, y, alpha);
	spriteIndex->draw(*Game::get().getRenderTarget(), { interpX, interpY }, imageIndex, { xScale, yScale }, sf::Color::White, imageAngle);
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

static BaseObject* ObjectCreate(
    const std::string& identifier,
    BaseObject* extends,
    LuaState& L, const std::filesystem::path& assets,
    const std::unordered_map<std::string, std::filesystem::path>& objectScriptPaths)
{
    auto& objMgr = ObjectManager::get();
    auto& gmlObjects = objMgr.gmlObjects;

    if (!identifier.empty()) {
        auto it = gmlObjects.find(identifier);
        if (it != gmlObjects.end()) {
            return it->second;
        }
    }

    std::unique_ptr<BaseObject> newObject = std::make_unique<BaseObject>(L);

    if (extends != nullptr) {
        newObject->parent = extends;
        /*
        for (auto& prop : extends->rawProperties) {
            newObject->rawProperties[prop.first] = { prop.second.first, prop.second.second };
        }
        for (std::pair p : extends->kvp) {
            newObject->kvp.insert(p);
        }
        */
    }

    /*
    sol::object globalObject = sol::make_object(lua, std::move(newObject));
    std::unique_ptr<BaseObject>& memPtr = globalObject.as<std::unique_ptr<BaseObject>&>();
    */

    if (!identifier.empty()) {
        // gmlObjects[identifier] = memPtr.get();
    }

    // memPtr->self = memPtr.get();
    // memPtr->identifier = identifier;

    // Run script for object
    auto it = objectScriptPaths.find(identifier);
    if (it != objectScriptPaths.end()) {
        auto res = LuaScript(L, it->second);
    }

    return NULL;
    // return memPtr.get();
}

ConvertType deduceType (const json& v) {
    if (v.is_number_integer())  return ConvertType::INTEGER;
    if (v.is_number())          return ConvertType::REAL;
    if (v.is_boolean())         return ConvertType::BOOLEAN;
    if (v.is_string())          return ConvertType::STRING;
    return ConvertType::STRING;
};

void addJSONPropsToObject(const json& props, BaseObject* obj) {
    for (auto& prop : props) {
        obj->rawProperties[prop["name"]] = { static_cast<ConvertType>(prop.value("type", static_cast<int>(deduceType(prop["value"])))), prop["value"] };
    }
}

static void ObjectTemplateCreateRecursive(
    std::string identifier,
    LuaState L,
    const std::filesystem::path& assets,
    const std::unordered_map<std::string, std::filesystem::path>& objectScriptPaths) {
    // potentially a parent class- ignore
    ObjectManager& objMgr = ObjectManager::get();
    if (objMgr.gmlObjects.find(identifier) != objMgr.gmlObjects.end()) {
        return;
    }

    std::filesystem::path p = assets / "managed" / "objects" / (identifier + ".json");
    if (!std::filesystem::exists(p)) {
        return;
    }

    std::ifstream i(p);
    json j = json::parse(i);

    BaseObject* obj = nullptr;
    if (!j["parent"].is_null()) {
        std::string parentIdentifier = j["parent"];
        auto it = objMgr.gmlObjects.find(parentIdentifier);
        if (it == objMgr.gmlObjects.end()) {
            ObjectTemplateCreateRecursive(parentIdentifier, L, assets, objectScriptPaths);
        }
        BaseObject* parentObj = objMgr.gmlObjects.at(parentIdentifier);
        obj = ObjectCreate(identifier, parentObj, L, assets, objectScriptPaths);
        addJSONPropsToObject(j["properties"], obj);
    }
    else {
        obj = ObjectCreate(identifier, nullptr, L, assets, objectScriptPaths);
        addJSONPropsToObject(j["properties"], obj);
    }

    obj->visible = j["visible"];

    if (!j["sprite"].is_null()) {
        obj->spriteIndex = &SpriteManager::get().sprites.at(j["sprite"]);
    }
}

std::unique_ptr<Object> ObjectManager::makeInstance(LuaState& L, BaseObject* baseObject) {
    if (baseObject == NULL) {
        auto copied = std::make_unique<Object>(L);
        return copied;
    }

    auto copied = std::make_unique<Object>(*baseObject);

    if (baseObject->parent) {
        std::deque<BaseObject*> parents;
        BaseObject* f = baseObject->parent;

        for (BaseObject* par = baseObject->parent; par; par = par->parent) {
            parents.push_back(par);
        }

        while (!parents.empty()) {
            BaseObject* p = parents.front();
            parents.pop_front();

            /*
            for (auto& v : p->kvp) {
                copied->kvp.insert(v);
            }
            */

            if (p->rawProperties.size() > 0) {
                /*
                sol::table t;
                if (copied->kvp.find("properties") != copied->kvp.end()) {
                    t = copied->getDyn("properties");
                } else {
                    t = copied->kvp.insert({ "properties", sol::table(lua, sol::create) }).first->second;
                }
                */
                for (auto& [k, v] : p->rawProperties) {
                    /*
                    t[k] = FieldCreateFromProperty(k, v.first, v.second, lua);
                    */
                }
            }

        }
    }
    
    /*
    for (auto& v : baseObject->kvp) {
        copied->setDyn(v.first, v.second);
    }
    */

    if (baseObject->rawProperties.size() > 0) {
        /*
        sol::table t;
        if (copied->kvp.find("properties") != copied->kvp.end()) {
            t = copied->getDyn("properties");
        } else {
            t = copied->kvp.insert({ "properties", sol::table(lua, sol::create) }).first->second;
        }
        */
        for (auto& [k, v] : baseObject->rawProperties) {
            // t[k] = FieldCreateFromProperty(k, v.first, v.second, lua);
        }
    }
    return copied;
}

/*
static inline float w_bb_left(const Object::Reference& r) { return r.object->bboxLeft(); }
static inline float w_bb_right(const Object::Reference& r) { return r.object->bboxRight(); }
static inline float w_bb_top(const Object::Reference& r) { return r.object->bboxTop(); }
static inline float w_bb_bottom(const Object::Reference& r) { return r.object->bboxBottom(); }
static inline void w_force_x(const Object::Reference& r, float x) {
    r.object->xPrevRender = r.object->xPrev = r.object->x = x;
}
static inline void w_force_y(const Object::Reference& r, float y) {
    r.object->yPrevRender = r.object->yPrev = r.object->y = y;
}
static inline bool w_extends(const Object::Reference& r, BaseObject* base) { return r.object->extends(base); }

static inline sol::object w_get(Object::Reference& r, const std::string& k) { return r.table[k]; }
static inline void w_set(Object::Reference& r, const std::string& k, sol::object v) { r.table[k] = v; }

#define WRAPPER_GET(lval, val, type) \
static inline type wGet_##lval(const Object::Reference& r) { return r.object->val; } \

#define WRAPPER_GET_SET(lval, val, type) \
static inline type wGet_##lval(const Object::Reference& r) { return r.object->val; } \
static inline void wSet_##lval(Object::Reference& r, type v) { r.object->val = v; }
WRAPPER_GET_SET(x, x, float)
WRAPPER_GET_SET(y, y, float)
WRAPPER_GET_SET(hspeed, xspd, float)
WRAPPER_GET_SET(vspeed, yspd, float)
WRAPPER_GET_SET(sprite_index, spriteIndex, SpriteIndex*)
WRAPPER_GET_SET(mask_index, maskIndex, SpriteIndex*)
WRAPPER_GET_SET(image_index, imageIndex, float)
WRAPPER_GET_SET(image_speed, imageSpeed, float)
WRAPPER_GET_SET(increment_image_speed, incrementImageSpeed, bool)
WRAPPER_GET_SET(image_angle, imageAngle, float)
WRAPPER_GET_SET(image_xscale, xScale, float)
WRAPPER_GET_SET(image_yscale, yScale, float)
WRAPPER_GET_SET(depth, depth, int)
WRAPPER_GET_SET(visible, visible, bool)
WRAPPER_GET_SET(active, active, bool)

WRAPPER_GET(super, parent, BaseObject*)
WRAPPER_GET(object_index, self, BaseObject*)
WRAPPER_GET(x_previous, xPrev, float)
WRAPPER_GET(y_previous, yPrev, float)
WRAPPER_GET(x_previous_render, xPrevRender, float)
WRAPPER_GET(y_previous_render, yPrevRender, float)
*/

void ObjectManager::initializeLua(LuaState& L, const std::filesystem::path &assets) {
    /*
    sol::table engineEnv = lua["TE"];

    auto objtype = lua.new_usertype<Object>(
        "Object", sol::no_constructor,

        "object_index", sol::readonly(&Object::self),
        "super",        sol::readonly(&Object::parent),

        sol::meta_function::index,      &Object::getDyn,
        sol::meta_function::new_index,  &Object::setDyn
    );

    lua.new_usertype<BaseObject>(
        "BaseObject",       sol::no_constructor,
        
        sol::meta_function::index,      &BaseObject::getDyn,
        sol::meta_function::new_index,  &BaseObject::setDyn,

        sol::base_classes, sol::bases<Object>()
    );

    auto ref = lua.new_usertype<Object::Reference>(
        "ObjectReference", sol::no_constructor,
        
        // fields
        "x",                        sol::property(wGet_x, wSet_x),
        "y",                        sol::property(wGet_y, wSet_y),
        "hspeed",                   sol::property(wGet_hspeed, wSet_hspeed),
        "vspeed",                   sol::property(wGet_vspeed, wSet_vspeed),
        "sprite_index",             sol::property(wGet_sprite_index, wSet_sprite_index),
        "mask_index",               sol::property(wGet_mask_index, wSet_mask_index),
        "image_index",              sol::property(wGet_image_index, wSet_image_index),
        "image_speed",              sol::property(wGet_image_speed, wSet_image_speed),
        "increment_image_speed",    sol::property(wGet_increment_image_speed, wSet_increment_image_speed),
        "image_angle",              sol::property(wGet_image_angle, wSet_image_angle),
        "image_xscale",             sol::property(wGet_image_xscale, wSet_image_xscale),
        "image_yscale",             sol::property(wGet_image_yscale, wSet_image_yscale),
        "depth",                    sol::property(wGet_depth, wSet_depth),
        "active",                   sol::property(wGet_active, wSet_active),
        "visible",                  sol::property(wGet_visible, wSet_visible),

        "object_index",             sol::readonly_property(wGet_object_index),
        "super",                    sol::readonly_property(wGet_super),
        "x_previous",               sol::readonly_property(wGet_x_previous),
        "y_previous",               sol::readonly_property(wGet_y_previous),
        "x_previous_render",        sol::readonly_property(wGet_x_previous_render),
        "y_previous_render",        sol::readonly_property(wGet_y_previous_render),

        // functions
        "bbox_left",    w_bb_left,
        "bbox_right",   w_bb_right,
        "bbox_top",     w_bb_top,
        "bbox_bottom",  w_bb_bottom,
        "force_x",      w_force_x,
        "force_y",      w_force_y,
        "is_a",         w_extends,
        "force_position", [](const Object::Reference& caller, float x, float y) {
            w_force_x(caller, x);
            w_force_y(caller, y);
        },
        
        sol::meta_function::index,      w_get,
        sol::meta_function::new_index,  w_set,
        sol::meta_function::equal_to, [](const Object::Reference& a, const Object::Reference& b) {
            return (a.object == b.object);
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

    engineEnv["object_create"] = [&](sol::variadic_args va) {
        // Has parent
        if (va.size() == 1 && va[0].is<std::unique_ptr<BaseObject>>()) {
            auto& parent = va[0].as<std::unique_ptr<BaseObject>&>();
            auto newObj = std::make_unique<BaseObject>(*(parent.get()));
            
            newObj->parent = parent.get();
            newObj->self = newObj.get();

            for (auto& prop : parent->rawProperties) {
                newObj->rawProperties[prop.first] = { prop.second.first, prop.second.second };
            }

            for (std::pair p : parent->kvp) {
                newObj->kvp.insert(p);
            }

            return std::move(newObj);
        }
        else if (va.size() >= 1 && va[0].is<std::string>()) {
            auto str = va[0].as<std::string>();
            
            auto newObj = std::make_unique<BaseObject>(lua);

            newObj->self = newObj.get();
            newObj->identifier = str;

            if (va.size() >= 2 && va[1].is<std::unique_ptr<BaseObject>>()) {
                auto& parent = va[1].as<std::unique_ptr<BaseObject>&>();
                newObj->parent = parent.get();

                for (auto& prop : parent->rawProperties) {
                    newObj->rawProperties[prop.first] = { prop.second.first, prop.second.second };
                }
                for (std::pair p : parent->kvp) {
                    newObj->kvp.insert(p);
                }
            }

            std::filesystem::path p = assets / "managed" / "objects" / (str + ".json");
            if (std::filesystem::exists(p)) {
                std::ifstream i(p);
                json j = json::parse(i);
                addJSONPropsToObject(j["properties"], newObj.get());
                if (!j["sprite"].is_null()) {
                    newObj->spriteIndex = &SpriteManager::get().sprites.at(j["sprite"]);
                }
                newObj->visible = j["visible"];
            }

            auto& objMgr = ObjectManager::get();
            objMgr.gmlObjects[str] = newObj.get();

            return std::move(newObj);
        }
        // No parent
        else {
            auto newObj = std::make_unique<BaseObject>(lua);
            newObj->self = newObj.get();
            return std::move(newObj);
        }
    };
    */

    /*
    for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "objects")) {
        if (!it.is_regular_file()) {
            continue;
        }
        std::string identifier = it.path().filename().replace_extension("").string();
        ObjectTemplateCreateRecursive(identifier, lua, assets, scriptPaths);
    }
    */

}