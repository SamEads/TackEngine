#pragma once

#include <iostream>
#include "object.h"
#include "collision.h"
#include "tilemap.h"
#include "roomreference.h"

class Background : public Object {
public:
    std::string name;
    bool tiledX, tiledY;
    bool offsetX, offsetY;
    sf::Color color = { 255, 255, 255, 255 };

    Background(LuaState L) : Object(L) {}
    void draw(Room* room, float alpha) override;
};

class Room {
public:
    class Camera {
    public:
        bool stayInBounds = true;
        Room* room = nullptr;
        float x = 0, y = 0;
        float xPrev = 0, yPrev = 0;
        float width = 0, height = 0;
        void setX(float x) {
            this->x = (!stayInBounds) ? x : std::clamp(x, 0.0f, room->width - width);
        }
        void setY(float y) {
            this->y = (!stayInBounds) ? y : std::clamp(y, 0.0f, room->height - height);
        }
        void teleport(float x, float y) {
            setX(x);
            setY(y);
            this->xPrev = this->x;
            this->yPrev = this->y;
        }
    };
public:
    const RoomReference* roomReference;
    std::vector<Object*> drawables {};
    void createAndRoomStartEvents(int roomIdx);
public:
    static void initializeLua(LuaState& L, const std::filesystem::path& assets);

    ObjectId myId = 0;
    ObjectId currentId = 0;
    
    std::vector<std::unique_ptr<Object>> instances {};
    std::vector<Background*> backgrounds {};
    std::vector<Tilemap*> tilemaps {};

    std::vector<std::unique_ptr<Object>> addQueue {};
    std::vector<size_t> deleteQueue {};
    
    std::unordered_map<ObjectId, Object*> ids {};

    LuaState L;

    int minReserved = 0;
    int width = 0, height = 0;
    Camera camera {};
    float renderCameraX = 0, renderCameraY = 0;

    Room(LuaState& L, RoomReference* data);
    Room(LuaState L);
    ~Room();

    void load(int roomIdx);

    void setView(float cx, float cy);

    void updateQueue() {
        // Add queued objects
        int size = instances.size();
        for (auto& o : addQueue) {
            o->vectorPos = size;
            instances.push_back(std::move(o));
            size++;
        }

        // Delete objects queued for deletion
        if (!deleteQueue.empty()) {
            std::sort(deleteQueue.begin(), deleteQueue.end(), std::greater<size_t>());
            for (auto& pos : deleteQueue) {
                size_t size = instances.size();
                if (pos >= size - 1) {
                    instances.pop_back();
                }
                else {
                    std::swap(instances[pos], instances[instances.size() - 1]);
                    luaL_unref(L, LUA_REGISTRYINDEX, instances.back()->tableReference);
                    instances.back()->hasTable = false;
                    instances.pop_back();
                    instances[pos]->vectorPos = pos;
                }
            }
            deleteQueue.clear();
        }

        addQueue.clear();
        deleteQueue.clear();
    }

    // TODO
    /*
    Object::Reference instancePlaceScript(Object* caller, float x, float y, sol::object type) {
        if (type == sol::lua_nil) {
            return Object::Reference { -1, this->myId, sol::make_object(lua, sol::lua_nil) };
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
                return Object::Reference { -1, this->myId, sol::make_object(lua, sol::lua_nil) };
            }
            if (!r.object->active) {
                return Object::Reference { -1, this->myId, sol::make_object(lua, sol::lua_nil) };
            }
            auto answer = polygonsIntersect(callerPts, r.object->getPoints());
            if (answer.intersect) {
                return r;
            }
        }

        auto& baseType = type.as<std::unique_ptr<BaseObject>&>();
        for (auto& i : ids) {
            if (i.second->active && i.second->extends(baseType.get())) {
                auto answer = polygonsIntersect(callerPts, i.second->getPoints());
                if (answer.intersect) {
                    return i.second->MyReference;
                }
            }
        }

        return Object::Reference { -1, 0, sol::make_object(lua, sol::lua_nil) };
    }
    */
};