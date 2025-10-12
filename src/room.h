#pragma once

#include <iostream>
#include "object.h"
#include "collision.h"

class Tileset;
class Tilemap : public Drawable {
public:
    std::vector<unsigned int> tileData;
    int tileCountX, tileCountY;
    Tileset* tileset;
    void draw(Room* room) override;
};

class Background : public Drawable {
public:
    SpriteIndex* spriteIndex;
    float speedX, speedY;
    bool tiledX, tiledY;
    bool offsetX, offsetY;
    float x, y;
    sf::Color color = sf::Color::White;
    void draw(Room* room) override;
};

class Room {
public:
    ObjectId currentId = 0;
    std::vector<std::unique_ptr<Tilemap>> tilemaps;
    std::vector<std::unique_ptr<Background>> backgrounds;
    std::vector<std::unique_ptr<Object>> instances;
    std::vector<std::unique_ptr<Object>> queuedInstances;
    std::vector<Object*> queuedDelete;
    std::unordered_map<ObjectId, Object*> ids;
    sol::state& lua;

    int width, height;
    float cameraX = 0, cameraY = 0;
    float cameraWidth = 0, cameraHeight = 0;

    Room(sol::state& lua, const std::string room);

    void update();

    void draw();

    float getCameraX() {
        return cameraX;
    }

    float getCameraY() {
        return cameraY;
    }

    void setCameraX(float val);

    void setCameraY(float val);

    void addQueue() {
        for (auto& o : queuedInstances) {
            instances.push_back(std::move(o));
        }
        queuedInstances.clear();

        for (auto& o : queuedDelete) {
            auto orig = std::find_if(instances.begin(), instances.end(), [o](const auto& unique) {
                return unique.get() == o;
                });
            if (orig != instances.end()) {
                instances.erase(orig);
            }
        }
        queuedDelete.clear();
    }

    void instanceDestroy(ObjectId id) {
        queuedDelete.push_back(ids[id]);
        ids.erase(id);
    }

    void instanceDestroyScript(Object::Reference ref) {
        queuedDelete.push_back(ids[ref.id]);
        ids.erase(ref.id);
    }

    bool objectExists(Object* baseType) {
        if (baseType == NULL) {
            return false;
        }
        auto it = std::find_if(ids.begin(), ids.end(), [baseType](const auto& o) {
            Object* optr = o.second;
            return optr->extends(baseType);
        });
        if (it != ids.end()) {
            return true;
        }
        return false;
    }

    bool instanceExists(Object::Reference reference) {
        int refId = reference.id;
        return ids.find(refId) != ids.end();
    }

    Object::Reference collisionRectangleScript(Object* caller, float x1, float y1, float x2, float y2, sol::object type) {
        if (type == sol::lua_nil) {
            return Object::Reference{ -1, sol::make_object(lua, sol::lua_nil) };
        }

        auto callerPts = caller->getPoints(x1, y1, x2, y2);

        if (type.is<Object::Reference>()) {
            Object::Reference& r = type.as<Object::Reference>();
            if (!instanceExists(r)) {
                return Object::Reference{ -1, sol::make_object(lua, sol::lua_nil) };
            }
            auto answer = polygonsIntersect(callerPts, r.object.as<Object*>()->getPoints());
            if (answer.intersect) {
                return r;
            }
        }

        auto& baseType = type.as<std::unique_ptr<Object>&>();
        for (auto& i : ids) {
            if (i.second->extends(baseType.get())) {
                auto answer = polygonsIntersect(callerPts, i.second->getPoints());
                if (answer.intersect) {
                    return i.second->makeReference();
                }
            }
        }

        return Object::Reference{ -1, sol::make_object(lua, sol::lua_nil) };
    }

    Object::Reference instancePlaceScript(Object* caller, float x, float y, sol::object type) {
        if (type == sol::lua_nil) {
            return Object::Reference { -1, sol::make_object(lua, sol::lua_nil) };
        }

        auto callerPts = caller->getPoints();
        for (auto& p : callerPts) {
            p.x -= caller->x;
            p.y -= caller->y;
            p.x += x;
            p.y += y;
        }

        if (type.is<Object::Reference>()) {
            Object::Reference& r = type.as<Object::Reference>();
            if (!instanceExists(r)) {
                return Object::Reference{ -1, sol::make_object(lua, sol::lua_nil) };
            }
            auto answer = polygonsIntersect(callerPts, r.object.as<Object*>()->getPoints());
            if (answer.intersect) {
                return r;
            }
        }

        auto& baseType = type.as<std::unique_ptr<Object>&>();
        for (auto& i : ids) {
            if (i.second->extends(baseType.get())) {
                auto answer = polygonsIntersect(callerPts, i.second->getPoints());
                if (answer.intersect) {
                    return i.second->makeReference();
                }
            }
        }

        return Object::Reference { -1, sol::make_object(lua, sol::lua_nil) };
    }

    Object::Reference instanceCreateScript(float x, float y, float depth, std::unique_ptr<Object>& baseObject) {
        Object* ptr = instanceCreate(x, y, depth, baseObject);

        auto it = ptr->kvp.find("create");
        if (it != ptr->kvp.end()) {
            auto res = it->second.as<sol::safe_function>()(ptr, this);
            if (!res.valid()) {
                sol::error e = res;
                std::cout << e.what() << "\n";
            }
        }
        ptr->xPrev = ptr->x;
        ptr->yPrev = ptr->y;

        return ptr->makeReference();
    }

    Object* instanceCreate(float x, float y, float depth, std::unique_ptr<Object>& baseObject) {
        std::unique_ptr<Object> copiedObject = ObjectManager::get().make(lua, baseObject);
        copiedObject->self = baseObject.get();
        copiedObject->id = currentId++;
        copiedObject->x = x;
        copiedObject->y = y;

        Object* ptr = copiedObject.get();

        queuedInstances.push_back(std::move(copiedObject));
        ids[ptr->id] = ptr;

        return ptr;
    }
};