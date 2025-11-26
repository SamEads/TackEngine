#pragma once

#include <iostream>
#include <deque>
#include <unordered_map>
#include <sol/sol.hpp>
#include <SFML/Graphics.hpp>
#include "vendor/json.hpp"
#include "sprite.h"
#include "util/mathhelper.h"
#include "drawable.h"
#include "objectid.h"

class Object;
class BaseObject;
class Room;

class Object : public Drawable {
public:
    struct Reference {
        ObjectId id;
        ObjectId roomId;
        sol::table table;
        Object* object;
    };
    Reference MyReference;

    sol::state& lua;
    // std::optional<sol::function> drawFunc;
    // std::optional<sol::function> stepFunc;
    std::unordered_map<std::string, sol::object> kvp;

    size_t vectorPos;
    float x = 0.0f, y = 0.0f;
    float xspd = 0.0f, yspd = 0.0f;
    float xPrev = 0.0f, yPrev = 0.0f;
    float xPrevRender = 0.0f, yPrevRender = 0.0f;
    float xScale = 1.0f, yScale = 1.0f;
    float imageIndex = 0.0f;
    float imageSpeed = 0.0f;
    float imageSpeedMod = 1.0f;
    float imageAngle = 0.0f;
    bool incrementImageSpeed = false;
    bool active = true;

    SpriteIndex* spriteIndex = nullptr;
    SpriteIndex* maskIndex = nullptr;

    BaseObject* parent = nullptr;
    BaseObject* self = nullptr;
    std::string identifier;

    Object(sol::state& lua) : lua(lua) {
        this->drawsGui = true;
    }

    // Retrieve the left side of the bounding box with scaling applied.
    const inline float bboxLeft() const {
        SpriteIndex* spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        const float hitboxLeft = spr->hitbox.position.x * fabsf(xScale);
        float originX = spr->originX * xScale;
        if (xScale < 0) {
            originX = (spr->width - spr->originX) * fabsf(xScale);
        }
        return x + hitboxLeft - originX;
    }

    const inline float bboxWidth() const {
        SpriteIndex* spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        return spr->hitbox.size.x * fabsf(xScale);
    }

    // Retrieve the right side of the bounding box with scaling applied.
    const inline float bboxRight() const {
        return bboxLeft() + bboxWidth();
    }

    // Retrieve the top of the bounding box with scaling applied.
    const inline float bboxTop() const {
        SpriteIndex* spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        const float hitboxTop = spr->hitbox.position.y * fabsf(yScale);
        float originY = spr->originY * yScale;
        if (yScale < 0) {
            originY = (spr->height - spr->originY) * fabsf(yScale);
        }
        return y + hitboxTop - originY;
    }

    const inline float bboxHeight() const {
        SpriteIndex* spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        return spr->hitbox.size.y * fabsf(yScale);
    }

    // Retrieve the bottom of the bounding box with scaling applied.
    const inline float bboxBottom() const {
        return bboxTop() + bboxHeight();
    }

    inline sf::FloatRect getRectangle() const {
        sf::FloatRect rect;
        rect.position = { bboxLeft(), bboxTop() };
        rect.size = { bboxRight() - rect.position.x, bboxBottom() - rect.position.y };
        return rect;
    }

    template <typename ... Args>
    bool runScript(const std::string& script, Args... args) {
        auto step = kvp.find(script);
        if (step != kvp.end()) {
            auto res = step->second.as<sol::safe_function>()(MyReference, args...);
            if (!res.valid()) {
                sol::error e = res;
                std::cout << e.what() << "\n";
            }
            else {
                return true;
            }
        }
        return false;
    }
    std::vector<sf::Vector2f> getPoints() const;
    void setDyn(const std::string& key, sol::main_object obj);
    sol::object getDyn(const std::string& ref);
    const bool extends(BaseObject* o) const;
    void draw(Room* room, float alpha) override;
    void beginDraw(Room* room, float alpha) override;
    void endDraw(Room* room, float alpha) override;
    void drawGui(Room* room, float alpha) override;
};

enum class ConvertType {
    REAL = 0,
    INTEGER = 1,
    STRING = 2,
    BOOLEAN = 3,
    EXPRESSION = 4,
    ASSET = 5,
    LIST = 6,
    COLOR = 7
};

inline sol::object FieldCreateFromProperty(const std::string& k, ConvertType type, const nlohmann::json& v, sol::state& lua) {
    sol::table engineEnv = lua["TE"];
    if ((type == ConvertType::ASSET || type == ConvertType::STRING) && engineEnv[v.get<std::string>()] != sol::lua_nil) {
        return engineEnv[v.get<std::string>()];
    }
    else if (type == ConvertType::INTEGER) {
        if (v.is_number()) {
            return sol::make_object(lua, v.get<int>());
        }
        return sol::make_object(lua, std::stoi(v.get<std::string>()));
    }
    else if (type == ConvertType::REAL) {
        if (v.is_number()) {
            return sol::make_object(lua, v.get<float>());
        }
        return sol::make_object(lua, std::stof(v.get<std::string>()));
    }
    else if (type == ConvertType::BOOLEAN) {
        if (v.is_boolean()) {
            return sol::make_object(lua, v.get<bool>());
        }
        bool boolValue = (v.get<std::string>() == "True") ? true : false;
        return sol::make_object(lua, boolValue);
    }
    else if (type == ConvertType::STRING && !v.get<std::string>().empty()) {
        return sol::make_object(lua, v.get<std::string>());
    }

    return sol::make_object(lua, sol::lua_nil);
};

class BaseObject : public Object {
public:
    std::unordered_map<std::string, std::pair<ConvertType, nlohmann::json>> rawProperties;
    BaseObject(sol::state& lua) : Object(lua) {}
};

class ObjectManager {
private:
    std::unordered_map<std::string, std::filesystem::path> scriptPaths;
public:
    /*
    class ScriptedInfo {
    public:
        std::unique_ptr<BaseObject>* objectPtr;
        std::function<std::unique_ptr<Object>(BaseObject*)> create;
    };
    */
    std::unordered_map<std::string, BaseObject*> gmlObjects;

    static ObjectManager& get() {
        static ObjectManager om;
        return om;
    }

    std::unique_ptr<Object> makeInstance(sol::state& lua, BaseObject* baseObject);

    void initializeLua(sol::state& lua, const std::filesystem::path& assets);
};