#pragma once

#include <iostream>
#include <deque>
#include <unordered_map>
#include <SFML/Graphics.hpp>
#include "vendor/json.hpp"
#include "../gfx/sprite.h"
#include "luainc.h"
#include "util/mathhelper.h"
#include "objectid.h"

class Object;
class BaseObject;
class Room;

enum class PropertyType {
    NIL = -1,
    REAL = 0,
    INTEGER = 1,
    STRING = 2,
    BOOLEAN = 3,
    EXPRESSION = 4,
    ASSET = 5,
    LIST = 6,
    COLOR = 7
};

int ObjectCreateLua(lua_State* L, bool luaOwned);

class Object {
public:
    int depth = 0;
    bool visible = true;

    struct Reference {
        ObjectId id;
        ObjectId roomId;
        Object* object;
    };
    Reference MyReference;

    LuaState L;
    bool hasTable = false;
    int tableReference;

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

    GFX::Sprite* spriteIndex = nullptr;
    GFX::Sprite* maskIndex = nullptr;

    Object* parent = nullptr;
    Object* self = nullptr;
    std::string identifier;

    std::map<std::string, std::pair<PropertyType, nlohmann::json>> baseProperties;

    Object(LuaState L) : L(L) {}

    ~Object () = default;

    // Retrieve the left side of the bounding box with scaling applied.
    const inline float getBboxLeft() const {
        auto spr = (maskIndex != nullptr) ? maskIndex : spriteIndex;
        if (identifier == "Player") {
            if (!spr) {
                std::cout << "Can't even start\n";
            }
        }
        if (!spr) return 0;
        const float hitboxLeft = spr->hitbox.position.x * fabsf(xScale);
        float originX = spr->originX * xScale;
        if (xScale < 0) {
            originX = (spr->width - spr->originX) * fabsf(xScale);
        }
        return x + hitboxLeft - originX;
    }

    const inline float bboxWidth() const {
        auto spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        return spr->hitbox.size.x * fabsf(xScale);
    }

    // Retrieve the right side of the bounding box with scaling applied.
    const inline float bboxRight() const {
        return getBboxLeft() + bboxWidth();
    }

    // Retrieve the top of the bounding box with scaling applied.
    const inline float bboxTop() const {
        auto spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        const float hitboxTop = spr->hitbox.position.y * fabsf(yScale);
        float originY = spr->originY * yScale;
        if (yScale < 0) {
            originY = (spr->height - spr->originY) * fabsf(yScale);
        }
        return y + hitboxTop - originY;
    }

    const inline float bboxHeight() const {
        auto spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        return spr->hitbox.size.y * fabsf(yScale);
    }

    // Retrieve the bottom of the bounding box with scaling applied.
    const inline float bboxBottom() const {
        return bboxTop() + bboxHeight();
    }

    inline sf::FloatRect getRectangle() const {
        sf::FloatRect rect;
        rect.position = { getBboxLeft(), bboxTop() };
        rect.size = { bboxRight() - rect.position.x, bboxBottom() - rect.position.y };
        return rect;
    }

    bool runScriptTimestep(const std::string& script, int roomIdx);
    bool runScriptDraw(const std::string& script, int roomIdx, float alpha);

    std::vector<sf::Vector2f> getPoints() const;
    const bool extends(Object* o) const;

    virtual void draw(Room* room, float alpha);
};

class ObjectManager {
public:
    std::unordered_map<std::string, std::pair<Object*, int>> tilemapObjects;
    static ObjectManager& get() {
        static ObjectManager om;
        return om;
    }

    void registerObject(const std::string& mapIdentifier, int luaRegistryRef, Object* innerUserdataPointer);
    void initializeLua(LuaState& L, const std::filesystem::path& assets);
};