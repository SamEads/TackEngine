#pragma once

#include <unordered_map>
#include <SFML/Graphics.hpp>
#include "vendor/lua/sol2/sol.hpp"
#include "sprite.h"
#include "mathhelper.h"

class Room;

class Drawable {
public:
    int depth = 0;
    bool visible = true;
    virtual void draw(Room* room) {}
};

using ObjectId = int;
class Object;
class Object : public Drawable {
public:
    template <typename ... Args>
    bool runScript(const std::string& script, Args... args) {
        auto step = kvp.find(script);
        if (step != kvp.end()) {
            auto res = step->second.as<sol::safe_function>()(this, args...);
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

    ObjectId id;

    struct Reference {
        ObjectId id;
        sol::object object;
    };
    virtual Reference makeReference() {
        return Reference { id, sol::make_object(lua, this) };
    }

    sol::state& lua;
    std::unordered_map<std::string, sol::object> kvp;

    float x = 0.0f, y = 0.0f;
    float xspd = 0.0f, yspd = 0.0f;
    float xPrev = 0.0f, yPrev = 0.0f;
    float xScale = 1.0f, yScale = 1.0f;
    float imageIndex = 0.0f;
    float imageSpeed = 0.0f;
    float imageSpeedMod = 1.0f;
    float imageAngle = 0.0f;

    SpriteIndex* spriteIndex = nullptr;
    SpriteIndex* maskIndex = nullptr;

    Object* parent = nullptr;
    Object* self = nullptr;
    std::string identifier;

    Object(sol::state& lua) : lua(lua) {}

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

    const inline float bboxRight() const {
        SpriteIndex* spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        const float hitboxWidth = spr->hitbox.size.x * fabsf(xScale);
        return bboxLeft() + hitboxWidth;
    }

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

    const inline float bboxBottom() const {
        SpriteIndex* spr = (maskIndex) ? maskIndex : spriteIndex;
        if (!spr) return 0;
        const float hitboxHeight = spr->hitbox.size.y * fabsf(yScale);
        return bboxTop() + hitboxHeight;
    }

    sf::FloatRect getRectangle() const {
        sf::FloatRect rect;
        rect.position = { bboxLeft(), bboxTop() };
        rect.size = { bboxRight() - rect.position.x, bboxBottom() - rect.position.y };
        return rect;
    }

    std::vector<sf::Vector2f> getPoints(float x1, float y1, float x2, float y2) const {
        sf::FloatRect rect = { { x1, y1 }, { x2 - x1, y2 - y1 } };
        return std::vector<sf::Vector2f> {
            { rect.position.x, rect.position.y },
            { rect.position.x + rect.size.x, rect.position.y },
            { rect.position.x + rect.size.x, rect.position.y + rect.size.y },
            { rect.position.x, rect.position.y + rect.size.y }
        };
    }

    std::vector<sf::Vector2f> getPoints() const {
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

    void setDyn(const std::string& key, sol::main_object obj) {
        auto it = kvp.find(key);
        if (it == kvp.end()) {
            kvp.insert({ key, sol::object(std::move(obj)) });
        }
        else {
            it->second = sol::object(std::move(obj));
        }
    }

    sol::object getDyn(const std::string& ref) {
        auto it = kvp.find(ref);
        if (it == kvp.end()) {
            return sol::lua_nil;
        }
        return it->second;
    }

    void trySet(sol::object v) {}

    const bool extends(Object* o) const {
        if (o == nullptr) {
            return false;
        }
        if (this->self == o) {
            return true;
        }
        if (parent == nullptr) {
            return false;
        }
        Object* check = parent;
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

    void beginStep(Room* room);
    void step(Room* room);
    void endStep(Room* room);
    void draw(Room* room) override;
};

#include <deque>
class ObjectManager {
public:
    class ScriptedInfo {
    public:
        sol::object object;
        std::function<std::unique_ptr<Object>(std::unique_ptr<Object>&)> create;
    };
    std::unordered_map<std::string, ScriptedInfo> baseClasses;

    static ObjectManager& get() {
        static ObjectManager om;
        return om;
    }

    std::unique_ptr<Object> make(sol::state& lua, std::unique_ptr<Object>& object) {
        if (object != NULL) {

            auto& list = baseClasses;

            auto it = std::find_if(list.begin(), list.end(), [&object](const std::pair<std::string, ScriptedInfo>& p) {
                sol::object obj = p.second.object;
                auto& uniquePtr = obj.as<std::unique_ptr<Object>&>();
                return uniquePtr.get() == object.get();
            });

            if (it != list.end()) {
                auto copied = it->second.create(object);

                std::deque<Object*> parents;
                Object* f = object->parent;
                if (f) {
                    while (true) {
                        if (f) {
                            parents.push_back(f);
                            f = f->parent;
                        }
                        else break;
                    }
                    while (!parents.empty()) {
                        Object* p = parents.front();
                        parents.pop_front();
                        for (auto& v : p->kvp) {
                            copied->setDyn(v.first, v.second);
                        }
                    }
                }
                for (auto& v : object->kvp) {
                    copied->setDyn(v.first, v.second);
                }
                return copied;
            }
        }

        // basic object, original didn't exist
        auto copied = std::make_unique<Object>(lua);
        return copied;
    }
};