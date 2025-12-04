#include <fstream>
#include "room.h"
#include "../game.h"
#include "../gfx/tileset.h"
#include "../vendor/json.hpp"

Room::Room(LuaState L) : L(L) {
    roomReference = nullptr;
    view.room = this;
}

Room::Room(LuaState& L, RoomReference* data) : Room(L) {
    roomReference = data;
}

Room::~Room() {
    for (auto& i : instances) {
        if (!i->hasTable) continue;
        int type = 0;
        if (dynamic_cast<Background*>(i.get()))
            type = 1;
        if (dynamic_cast<Tilemap*>(i.get()))
            type = 2;
        std::string vna = std::string((type==0)?"instance":((type==1)?"background":"tilemap"));
        lua_unreference(L, i->tableReference, vna);
        i->hasTable = false;
    }
}

void Room::initializeLua(LuaState& lua, const std::filesystem::path &assets) {
    RoomInitializeLua(lua, assets);
    RoomViewInitializeLua(lua, assets);
    BackgroundInitializeLua(lua, assets);
    TilemapInitializeLua(lua, assets);
}

void Room::load(int roomIdx) {
    using namespace nlohmann;

    auto& game = Game::get();
    auto& objMgr = ObjectManager::get();
    auto& lua = game.L;

    view.width = game.canvasWidth;
    view.height = game.canvasHeight;

    if (roomReference == nullptr) {
        return;
    }

    auto jsonPath = roomReference->p;
    auto binPath = jsonPath.replace_extension(".bin");
    std::ifstream in(binPath, std::ios::binary);

    int settingsWidth, settingsHeight;
    in.read(reinterpret_cast<char*>(&width), sizeof(width));
    in.read(reinterpret_cast<char*>(&height), sizeof(height));

    int layerCount;
    in.read(reinterpret_cast<char*>(&layerCount), sizeof(layerCount));

    auto readstr = [&]() {
        int strLen;
        in.read(reinterpret_cast<char*>(&strLen), sizeof(strLen));

        std::string nameStr;
        nameStr.resize(strLen);

        nameStr[strLen] = '\0';
        in.read(&nameStr[0], strLen);

        return nameStr;
    };

    std::string namestr = readstr();
    for (int i = 0; i < layerCount; ++i) {
        std::string type = readstr();
        std::string name = readstr();

        int depth;
        bool visible;

        in.read(reinterpret_cast<char*>(&depth), sizeof(depth));
        in.read(reinterpret_cast<char*>(&visible), sizeof(visible));

        if (type == "background") {
            std::unique_ptr<Background> bg = std::make_unique<Background>(lua);

            bg->name = name;
            bg->visible = visible;
            bg->depth = depth;

            lua_newtable(L);
                lua_pushstring(L, "__cpp_ptr");
                    lua_pushlightuserdata(L, bg.get());
                lua_rawset(L, -3);
                luaL_setmetatable(L, "Background");

            int regIdx = lua_reference(L, "background");

            bg->hasTable = true;
            bg->tableReference = regIdx;

            in.read(reinterpret_cast<char*>(&bg->tiledX), sizeof(bg->tiledX));
            in.read(reinterpret_cast<char*>(&bg->tiledY), sizeof(bg->tiledY));
            in.read(reinterpret_cast<char*>(&bg->xspd), sizeof(bg->xspd));
            in.read(reinterpret_cast<char*>(&bg->yspd), sizeof(bg->yspd));
            in.read(reinterpret_cast<char*>(&bg->x), sizeof(bg->x));
            in.read(reinterpret_cast<char*>(&bg->y), sizeof(bg->y));

            in.read(reinterpret_cast<char*>(&bg->color.r), sizeof(char) * 4);

            bool hasSprite;
            in.read(reinterpret_cast<char*>(&hasSprite), sizeof(hasSprite));
            if (hasSprite) {
                bg->spriteIndex = GFX::sprites[readstr()].get();
            }

            bg->MyReference.id = currentId++;
            backgrounds.push_back(bg.get());
            bg->vectorPos = instances.size();

            auto bgPtr = bg.get();
            instances.push_back(std::move(bg));
            // ids[bg->MyReference.id] = bgPtr;
        }

        else if (type == "tiles") {
            std::unique_ptr<Tilemap> map = std::make_unique<Tilemap>(lua);
            map->name = name;
            map->depth = depth;
            map->visible = visible;

            lua_newtable(L);
                lua_pushstring(L, "__cpp_ptr");
                lua_pushlightuserdata(L, map.get());
                lua_rawset(L, -3);
                luaL_setmetatable(L, "Tilemap");
            int regIdx = lua_reference(L, "tilemap");

            map->hasTable = true;
            map->tableReference = regIdx;

            bool compressed;
            in.read(reinterpret_cast<char*>(&compressed), sizeof(compressed));

            int width, height;
            in.read(reinterpret_cast<char*>(&map->tileCountX), sizeof(map->tileCountX));
            in.read(reinterpret_cast<char*>(&map->tileCountY), sizeof(map->tileCountY));

            if (compressed) {
                size_t tileArrSize;
                in.read(reinterpret_cast<char*>(&tileArrSize), sizeof(tileArrSize));

                int32_t* tiles = new int32_t[tileArrSize];

                in.read((char*)tiles, tileArrSize * sizeof(int32_t));

                auto& decompressed = map->tileData;
		        decompressed.reserve(map->tileCountX * map->tileCountY);

                int size = tileArrSize;
                for (int j = 0; j < size;) {
                    int value = tiles[j++];

                    // start a value train
                    if (value >= 0) {
                        while (true) {
                            // stay in bounds
                            if (j >= size) {
                                break;
                            }

                            int nextValue = tiles[j++];

                            if (nextValue >= 0) {
                                decompressed.push_back(nextValue);
                            }
                            else {
                                value = nextValue;
                                break;
                            }
                        }
                    }

                    // Negative value is count
                    if (value < 0) {
                        // stay in bounds
                        if (j >= size) {
                            break;
                        }

                        int repeatValue = tiles[j++];

                        for (int k = 0; k < -value; ++k) {
                            decompressed.push_back(repeatValue);
                        }
                    }
                }

                delete[] tiles;
            }
            else {
                size_t tileArrSize;
                in.read(reinterpret_cast<char*>(&tileArrSize), sizeof(tileArrSize));

                map->tileData.resize(tileArrSize);
                unsigned int* ptr = map->tileData.data();

                in.read((char*)ptr, tileArrSize * sizeof(int32_t));
            }

            std::string tilesetRes = readstr();

            map->tileset = &TilesetManager::get().tilesets[tilesetRes];

            map->MyReference.id = currentId++;
            tilemaps.push_back(map.get());
            map->vectorPos = instances.size();
            
            auto mapPtr = map.get();
            instances.push_back(std::move(map));
            // ids[map->MyReference.id] = mapPtr;
        }

        else if (type == "objects") {
            int objectCount;
            in.read(reinterpret_cast<char*>(&objectCount), sizeof(objectCount));

            std::vector<std::string> vec;
            vec.reserve(objectCount);
            for (int i = 0; i < objectCount; ++i) {
                vec.emplace_back(readstr());
            }

            size_t instanceCount;
            in.read(reinterpret_cast<char*>(&instanceCount), sizeof(instanceCount));

            for (int i = 0; i < instanceCount; ++i) {
                int objNamePos;
                in.read(reinterpret_cast<char*>(&objNamePos), sizeof(objNamePos));
                std::string& name = vec[objNamePos];

                float x;
                in.read(reinterpret_cast<char*>(&x), sizeof(x));

                float y;
                in.read(reinterpret_cast<char*>(&y), sizeof(y));

                Object* ptr = nullptr;
                std::unique_ptr<Object> scrappedPtr = nullptr;

                auto it = objMgr.tilemapObjects.find(name);
                if (it != objMgr.tilemapObjects.end()) {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.second); // idx
                    int objIdx = lua_gettop(L);
                    int objectId = currentId++;

                        // Fetch original object
                        Object* original = lua_toclass<Object>(L, objIdx);

                        std::unique_ptr<Object> o = std::make_unique<Object>(*original);
                        ptr = o.get();
                            PushNewInstance(L, objIdx, objectId, ptr, original);
                        int tableIdx = lua_reference(L, "instance"); // idx
                        o->hasTable = true;
                        o->tableReference = tableIdx;
                    lua_pop(L, 1); // =

                    ptr->x = x;
                    ptr->y = y;
                    ptr->depth = depth;

                    ptr->MyReference.id = objectId;
                    ptr->MyReference.roomId = myId;
                    ptr->MyReference.object = ptr;
                    
                    addQueue.push_back(std::move(o));
                    ids[objectId] = ptr;
                }
                else {
                    scrappedPtr = std::make_unique<Object>(L);
                    ptr = scrappedPtr.get();
                }

                bool readAdvanced = false;
                in.read(reinterpret_cast<char*>(&readAdvanced), sizeof(readAdvanced));

                if (readAdvanced) {
                    float& rotation = ptr->imageAngle;
                    in.read(reinterpret_cast<char*>(&rotation), sizeof(rotation));

                    float& imageIndex = ptr->imageIndex;
                    in.read(reinterpret_cast<char*>(&imageIndex), sizeof(imageIndex));

                    float& imageSpeed = ptr->imageSpeedMod;
                    in.read(reinterpret_cast<char*>(&imageSpeed), sizeof(imageSpeed));

                    float& scaleX = ptr->xScale;
                    in.read(reinterpret_cast<char*>(&scaleX), sizeof(scaleX));

                    float& scaleY = ptr->yScale;
                    in.read(reinterpret_cast<char*>(&scaleY), sizeof(scaleY));

                    uint8_t c;
                    for (int i = 0; i < 4; ++i)
                    in.read(reinterpret_cast<char*>(&c), sizeof(uint8_t));
                }
                int propertyCount;
                in.read(reinterpret_cast<char*>(&propertyCount), sizeof(propertyCount));

                if (propertyCount > 0) {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, ptr->tableReference); // reg index
                    lua_pushstring(L, "properties"); // prop str, reg index
                    lua_rawget(L, -2); // properties table, reg index
                    lua_remove(L, -2); // properties
                    for (int j = 0; j < propertyCount; ++j) {
                        std::string key = readstr();

                        uint8_t type;
                        in.read(reinterpret_cast<char*>(&type), sizeof(type));

                        if (type == 0) { // float
                            float val;
                            in.read(reinterpret_cast<char*>(&val), sizeof(val));
                            lua_pushnumber(L, val);
                        }
                        else if (type == 1) { // int
                            int val;
                            in.read(reinterpret_cast<char*>(&val), sizeof(val));
                            lua_pushinteger(L, val);
                        }
                        else if (type == 2) { // bool
                            bool val;
                            in.read(reinterpret_cast<char*>(&val), sizeof(val));
                            lua_pushboolean(L, val);
                        }
                        else { // other
                            std::string val = readstr();
                            if (val.empty()) {
                                lua_pushnil(L);
                            }
                            else {
                                lua_getglobal(L, ENGINE_ENV);
                                lua_getfield(L, -1, val.c_str());
                                if (lua_isnil(L, -1)) {
                                    lua_pop(L, 2);
                                    lua_pushstring(L, val.c_str());
                                }
                                else {
                                    int type = lua_type(L, -1);
                                    lua_remove(L, -2);
                                }
                            }
                        }
                        lua_setfield(L, -2, key.c_str());
                    }
                    lua_pop(L, 1);
                }
            }
        }
    }
    createAndRoomStartEvents(roomIdx);
}

// Instances -> "Create"
// Instances -> "Room Start"
void Room::createAndRoomStartEvents(int roomIdx) {
    auto& game = Game::get();
    updateQueue();

    for (auto& objUnique : instances) {
        objUnique->runScriptTimestep("create", roomIdx);
    }
    updateQueue();

    for (auto& objUnique : instances) {
        objUnique->runScriptTimestep("room_start", roomIdx);
    }
    updateQueue();

    // Update prev vals

    view.xPrev = view.x;
    view.yPrev = view.y;

    for (auto& ptr : instances) {
        ptr->xPrevRender = ptr->xPrev = ptr->x;
        ptr->yPrevRender = ptr->yPrev = ptr->y;
    }
}

void Room::setView(float cx, float cy) {
    auto target = Game::get().getRenderTarget();
    auto targetSize = target->getSize();
    float targetWidth = targetSize.x;
    float targetHeight = targetSize.y;

    sf::View view(sf::FloatRect { { 0.0f, 0.0f }, { targetWidth, targetHeight } });

    view.setCenter({ cx + targetWidth / 2.0f, cy + targetHeight / 2.0f });
    target->setView(view);

    renderCameraX = cx;
    renderCameraY = cy;
}