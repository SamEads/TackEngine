#pragma once

#include <iostream>
#include "../object/object.h"
#include "tilemap.h"
#include "roomreference.h"

void RoomInitializeLua(lua_State* L, const std::filesystem::path& assets);
void RoomViewInitializeLua(lua_State* L, const std::filesystem::path& assets);
void BackgroundInitializeLua(lua_State* L, const std::filesystem::path& assets);
void TilemapInitializeLua(lua_State* L, const std::filesystem::path& assets);

int PushNewInstance(lua_State* L, int originalTableIndex, ObjectId objectId, Object* instance, Object* pseudoclass);

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
    class View {
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

    LuaState L;

    ObjectId myId = 0;
    ObjectId currentId = 0;
    
    std::vector<std::unique_ptr<Object>> instances {};
    std::vector<Background*> backgrounds {};
    std::vector<Tilemap*> tilemaps {};

    std::vector<std::unique_ptr<Object>> addQueue {};
    std::vector<size_t> deleteQueue {};
    
    std::unordered_map<ObjectId, Object*> ids {};

    int width = 0;
    int height = 0;
    View view {};

    Room(LuaState& L, RoomReference* data);
    Room(LuaState L);
    ~Room();

    void load(int roomIdx);

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
                int type = 0;
                if (dynamic_cast<Background*>(instances[pos].get()))
                    type = 1;
                if (dynamic_cast<Tilemap*>(instances[pos].get()))
                    type = 2;
                std::string vna = std::string((type == 0) ? "instance" : ((type == 1) ? "background" : "tilemap"));
                size_t size = instances.size();
                if (pos >= size - 1) {
                    lua_unreference(L, instances[pos]->tableReference, vna);
                    instances[pos]->hasTable = false;

                    instances.pop_back();
                }
                else {
                    lua_unreference(L, instances[pos]->tableReference, vna);
                    instances[pos]->hasTable = false;

                    std::swap(instances[pos], instances[instances.size() - 1]);

                    instances.pop_back();
                    instances[pos]->vectorPos = pos;
                }
            }
            deleteQueue.clear();
        }

        addQueue.clear();
        deleteQueue.clear();
    }
};