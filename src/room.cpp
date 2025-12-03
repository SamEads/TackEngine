#include <fstream>
#include "room.h"
#include "game.h"
#include "tileset.h"
#include "vendor/json.hpp"

Room::Room(LuaState L) : L(L) {
    roomReference = nullptr;
    camera.room = this;
}

Room::~Room() {
    for (auto& i : instances) {
        if (!i->hasTable) continue;
        luaL_unref(L, LUA_REGISTRYINDEX, i->tableReference);
        i->hasTable = false;
    }
}

Room::Room(LuaState& L, RoomReference* data) : Room(L) {
    roomReference = data;
}

Background* RoomGetBG(Room* room, const std::string& str) {
    for (auto& it : room->backgrounds) {
        if (it->name == str) {
            return it;
        }
    }
    return nullptr;
}

Tilemap* RoomGetTilemap(Room* room, const std::string& str) {
    for (auto& it : room->tilemaps) {
        if (it->name == str) {
            return it;
        }
    }
    return nullptr;
}

static int RoomGet(lua_State* L) {
    const char* key = luaL_checkstring(L, 2);
    Room* room = lua_toclass<Room>(L, 1);

    if (strcmp("width", key) == 0) {
        lua_pushnumber(L, room->width);
        return 1;
    }

    if (strcmp("height", key) == 0) {
        lua_pushnumber(L, room->height);
        return 1;
    }

    if (strcmp("render_x", key) == 0) {
        lua_pushnumber(L, room->renderCameraX);
        return 1;
    }

    if (strcmp("render_y", key) == 0) {
        lua_pushnumber(L, room->renderCameraY);
        return 1;
    }

    lua_pushvalue(L, 2); // push key
    lua_rawget(L, 1); // consumes key

    if (!lua_isnil(L, -1)) {
        return 1;
    }

    lua_pop(L, 1);
    lua_getmetatable(L, 1); // mt
    lua_pushvalue(L, 2); // key,mt
    lua_gettable(L, -2); // val,mt
    if (!lua_isnil(L, -1)) {
        lua_remove(L, -2); // remove mt
        return 1;
    }

    lua_pop(L, 2);
    return 0;
}

int RoomStep(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);

    room->camera.xPrev = room->camera.x;
    room->camera.yPrev = room->camera.y;

    Game& game = Game::get();
    room->camera.width = game.canvasWidth;
    room->camera.height = game.canvasHeight;

    for (auto& i : room->instances) {
        if (i->active) {
            i->xPrev = i->x;
            i->yPrev = i->y;
            i->xPrevRender = i->x;
            i->yPrevRender = i->y;
            if (i->incrementImageSpeed) {
                i->imageIndex += (i->imageSpeed * i->imageSpeedMod);
            }
        }
    }

    // Begin Step
    for (auto& instance : room->instances) {
        if (instance->active && instance->hasTable) {
            instance->runScriptTimestep("begin_step", 1);
        }
    }
    room->updateQueue();

    // Step
    for (auto& instance : room->instances) {
        if (instance->active && instance->hasTable) {
            instance->runScriptTimestep("step", 1);
        }
    }
    room->updateQueue();

    // End Step
    for (auto& instance : room->instances) {
        if (instance->active) {
            instance->runScriptTimestep("end_step", 1);
        }
    }
    room->updateQueue();

    return 0;
}

int RoomDraw(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);

    float alpha = luaL_checknumber(L, 2);

    room->drawables.clear();

    int count = 0;
    for (auto& i : room->instances) {
        if (i->visible && i->active && dynamic_cast<Tilemap*>(i.get())) {
            count++;
        }
    }
    for (auto& i : room->instances) {
        if (i->visible && i->active) {
            room->drawables.push_back(i.get());
        }
    }

    std::sort(room->drawables.begin(), room->drawables.end(), [](const Drawable* a, const Drawable* b) {
        return a->depth > b->depth;
    });

    for (auto& d : room->drawables) {
        if (!d->active || !d->visible) continue;
        if (d->hasTable) {
            if (!d->runScriptDraw("begin_draw", 1, alpha)) {
                d->beginDraw(room, alpha);
            }
        }
        else {
            d->beginDraw(room, alpha);
        }
    }

    for (auto& d : room->drawables) {
        if (!d->active || !d->visible) continue;
        if (d->hasTable) {
            if (!d->runScriptDraw("draw", 1, alpha)) {
                d->draw(room, alpha);
            }
        }
        else {
            d->draw(room, alpha);
        }
    }
    
    for (auto& d : room->drawables) {
        if (!d->active || !d->visible) continue;
        if (d->hasTable) {
            if (!d->runScriptDraw("end_draw", 1, alpha)) {
                d->endDraw(room, alpha);
            }
        }
        else {
            d->endDraw(room, alpha);
        }
    }

    auto target = Game::get().getRenderTarget();
    target->setView(target->getDefaultView());

    for (auto& d : room->drawables) {
        if (!d->active || !d->visible) continue;
        if (d->hasTable) {
            if (!d->runScriptDraw("draw_gui", 1, alpha)) {
                d->drawGui(room, alpha);
            }
        }
        else {
            d->drawGui(room, alpha);
        }
    }

    return 0;
}

int RoomSet(lua_State* L) {
    const char* key = luaL_checkstring(L, 2);
    Room* room = lua_toclass<Room>(L, 1);

    if (strcmp(key, "width") == 0) {
        room->width = luaL_checknumber(L, 3);
        return 0;
    }

    lua_pushvalue(L, 2);    // k
    lua_pushvalue(L, 3);    // v
    lua_rawset(L, 1);

    return 0;
}

static int RoomGarbageCollect(lua_State* L) {

    LuaState& lua = LuaState::get(L);

    return 0;
}

static int L_ROOM_CREATE(lua_State* L) {
    LuaState& lua = LuaState::get(L);
    lua_newtable(L); // -1: table
        // Create and call constructor for room
        Room* room = nullptr;
        // No room reference
        if (lua_gettop(lua) == 1 || lua_isnil(L, 1)) {
            room = new(lua_newuserdata(lua, sizeof(Room))) Room(lua); // -1: room, -2: table
        }
        else {
            // Initiate with room reference
            RoomReference* ref = (RoomReference*)lua_touserdata(L, 1);
            room = new(lua_newuserdata(lua, sizeof(Room))) Room(lua, ref); // -1: room, -2: table
        }

        lua_newtable(L);
            lua_pushcfunction(L, [](lua_State* L) -> int {
                static_cast<Room*>(lua_touserdata(L, 1))->~Room();
                return 0;
            });
            lua_setfield(L, -2, "__gc");
        lua_setmetatable(L, -2); // meta

        lua_setfield(L, -2, "__cpp_ptr"); // -1: table

        // Create view
        lua_newtable(L); // -1: this, -2: main table
            Room::Camera* camera = &room->camera;
            lua_pushlightuserdata(L, camera);       // Light userdata, the camera is technically already being GC'd by Lua since the Room is Lua-owned
            lua_setfield(L, -2, "__cpp_ptr");   // This works fine since I do tables really weird and don't let Lua and C++ know everything about each others contexts..
            
            lua_pushcfunction(L, [](lua_State* L) -> int {
                Room::Camera* camera = lua_toclass<Room::Camera>(L, 1);
                lua_pushnumber(L, camera->width);
                return 1;
            });
            lua_setfield(L, -2, "get_width");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Room::Camera* camera = lua_toclass<Room::Camera>(L, 1);
                lua_pushnumber(L, camera->height);
                return 1;
            });
            lua_setfield(L, -2, "get_height");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Room::Camera* camera = lua_toclass<Room::Camera>(L, 1);
                lua_pushnumber(L, camera->x);
                return 1;
            });
            lua_setfield(L, -2, "get_x");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Room::Camera* camera = lua_toclass<Room::Camera>(L, 1);
                lua_pushnumber(L, camera->xPrev);
                return 1;
            });
            lua_setfield(L, -2, "get_x_previous");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Room::Camera* camera = lua_toclass<Room::Camera>(L, 1);
                lua_pushnumber(L, camera->y);
                return 1;
            });
            lua_setfield(L, -2, "get_y");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Room::Camera* camera = lua_toclass<Room::Camera>(L, 1);
                lua_pushnumber(L, camera->yPrev);
                return 1;
            });
            lua_setfield(L, -2, "get_y_previous");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_toclass<Room::Camera>(L, 1)->setX(luaL_checknumber(L, 2));
                return 0;
            });
            lua_setfield(L, -2, "set_x");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_toclass<Room::Camera>(L, 1)->setY(luaL_checknumber(L, 2));
                return 0;
            });
            lua_setfield(L, -2, "set_y");
            
            luaL_setmetatable(L, "__te_view");
        lua_setfield(L, -2, "view"); // Set table as "view" field

        luaL_setmetatable(lua, "__te_room"); // -1: room table

    lua_pushvalue(L, -1); // room, room
    int roomIdx = lua_gettop(L);

    room->load(roomIdx);

    lua_pop(L, 1);

    return 1; // return room
}

static int RoomInstancesRect(lua_State* L) {
    int argcount = lua_gettop(L);

    Room* room = lua_toclass<Room>(L, 1);
    float left = lua_tonumber(L, 3);
    float top = lua_tonumber(L, 4);
    float right = lua_tonumber(L, 5);
    float bottom = lua_tonumber(L, 6);

    float width = right - left;
    float height = bottom - top;

    sf::FloatRect rect = { { left, top }, { width, height } };

    lua_getfield(L, 2, "__id");
    bool inst = lua_isnil(L, -1);
    lua_pop(L, 1); // pop nil instance

    const Object* ignore = (argcount < 8) ? nullptr : lua_toclass<Object>(L, 8);
    Object* base = (inst) ? lua_toclass<Object>(L, 7)->self : lua_toclass<Object>(L, 7);
    lua_newtable(L);
    int count = 0;
    for (auto& i : room->ids) {
        auto& instance = i.second;
        if (i.second->hasTable && i.second->active && i.second->extends(base)) {
            if (ignore == nullptr || i.second != ignore) {
                sf::FloatRect otherRect = { { instance->getBboxLeft(), instance->bboxTop() }, { 0, 0 } };
                otherRect.size.x = instance->bboxRight() - otherRect.position.x;
                otherRect.size.y = instance->bboxBottom() - otherRect.position.y;
                auto intersection = rect.findIntersection(otherRect);
                if (intersection.has_value()) {
                    count++;
                    lua_rawgeti(L, LUA_REGISTRYINDEX, instance->tableReference);
                    lua_rawseti(L, -2, count);
                }
            }
        }
    }
    return 1;
}

// 1:room, 2:instance, 3:left, 4:top, 5:right, 6:bottom, 7:object
static int RoomInstanceRect(lua_State* L) {
    int argcount = lua_gettop(L);

    Room* room = lua_toclass<Room>(L, 1);
    float left = lua_tonumber(L, 3);
    float top = lua_tonumber(L, 4);
    float right = lua_tonumber(L, 5);
    float bottom = lua_tonumber(L, 6);

    float width = right - left;
    float height = bottom - top;

    sf::FloatRect rect = { { left, top }, { width, height } };

    lua_getfield(L, 7, "__id");
    bool isInstance = !lua_isnil(L, -1);
    if (isInstance) {
        int instanceId = lua_tointeger(L, -1);
        lua_pop(L, 1);

        auto it = room->ids.find(instanceId);
        if (it == room->ids.end()) {
            lua_pushnil(L); // nil
            return 1;
        }
        
        Object* foundInstance = it->second;
        if (!foundInstance->active || !foundInstance->hasTable) {
            lua_pushnil(L); // nil
            return 1;
        }

        sf::FloatRect otherRect = { { foundInstance->getBboxLeft(), foundInstance->bboxTop() }, { 0, 0 } };
        otherRect.size.x = foundInstance->bboxRight() - otherRect.position.x;
        otherRect.size.y = foundInstance->bboxBottom() - otherRect.position.y;
        auto intersection = rect.findIntersection(otherRect);
        
        // Intersection vs none
        if (intersection.has_value()) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, foundInstance->tableReference);
            return 1;
        }
        else {
            lua_pushnil(L);
            return 1;
        }
    }
    else {
        lua_pop(L, 1); // pop nil instance

        Object* base = lua_toclass<Object>(L, 7);
        const Object* ignore = (argcount >= 8) ? lua_toclass<Object>(L, 8) : nullptr;

        for (auto& i : room->ids) {
            auto& instance = i.second;
            if (i.second->hasTable &&
                i.second->active &&
                i.second->extends(base)) {
                if (ignore == nullptr || i.second != ignore) {
                    sf::FloatRect otherRect = { { instance->getBboxLeft(), instance->bboxTop() }, { 0, 0 } };
                    otherRect.size.x = instance->bboxRight() - otherRect.position.x;
                    otherRect.size.y = instance->bboxBottom() - otherRect.position.y;
                    auto intersection = rect.findIntersection(otherRect);
                    if (intersection.has_value()) {
                        lua_rawgeti(L, LUA_REGISTRYINDEX, instance->tableReference);
                        return 1;
                    }
                }
            }
        }
        lua_pushnil(L);
        return 1;
    }
}

static int RoomInstanceExists(lua_State* L) {
    if (lua_gettop(L) < 2 || lua_isnil(L, 2)) {
        lua_pushboolean(L, false);
        return 1;
    }

    Room* room = lua_toclass<Room>(L, 1);
    Object* object = lua_toclass<Object>(L, 2);

    if (object == nullptr) {
        lua_pushboolean(L, false);
        return 1;
    }

    // Instance
    if (lua_getfieldexists(L, 2, "__id")) {
        lua_getfield(L, 2, "__id");
            int id = lua_tointeger(L, -1);
        lua_pop(L, 1);
        bool exists = room->ids.find(id) != room->ids.end() && object->MyReference.roomId == room->myId;
        lua_pushboolean(L, exists);
        return 1;
    }
    // Class
    else for (auto& [k, other] : room->ids) {
        if (other->extends(object)) {
            lua_pushboolean(L, true);
            return 1;
        }
    }

    lua_pushboolean(L, false);
    return 1;
}

static int RoomInstanceDestroy(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);

    bool inst = false;
    lua_getfield(L, 2, "__id");
    inst = !lua_isnil(L, -1);

    // Unique instance
    if (inst) {
        int id = lua_tonumber(L, -1);
        lua_pop(L, 1);

        auto idPos = room->ids.find(id);
        if (idPos != room->ids.end()) {
            auto object = idPos->second;
            object->runScriptTimestep("destroy", 1);
            room->deleteQueue.push_back(object->vectorPos);
            room->ids.erase(object->MyReference.id);
        }
    }

    // Whole object
    else {
        lua_pop(L, 1);

        Object* original = lua_toclass<Object>(L, 1);
        for (auto& [k, i] : room->ids) {
            if (i->extends(original)) {
                ObjectId id = i->MyReference.id;
                auto idPos = room->ids.find(id);
                if (idPos != room->ids.end()) {
                    room->deleteQueue.push_back(i->vectorPos);
                    room->ids.erase(id);
                }
            }
        }
    }

    return 0;
}

static int lua_pushnewinstance(lua_State* L, int originalTableIndex, ObjectId objectId, Object* instance, Object* pseudoclass) {
    lua_newtable(L); // table

        lua_pushnumber(L, objectId);    // id, table
        lua_setfield(L, -2, "__id");    // table

        if (pseudoclass->parent != nullptr)
            lua_rawgeti(L, LUA_REGISTRYINDEX, pseudoclass->parent->tableReference); // super, table
        else
            lua_pushnil(L); // nil, table
        lua_setfield(L, -2, "super");           // table

        if (pseudoclass->spriteIndex != nullptr) {
            lua_pushstring(L, "sprite_index");
            lua_rawgeti(L, LUA_REGISTRYINDEX, instance->spriteIndex->ref);
            lua_rawset(L, -3);

            instance->spriteIndex = pseudoclass->spriteIndex;
        }

        lua_pushstring(L, "properties");
        lua_newtable(L);
            for (auto& property : pseudoclass->baseProperties) {
                const std::string& name = property.first;
                PropertyType type = property.second.first;
                nlohmann::json& data = property.second.second;

                switch (type) {
                    case (PropertyType::BOOLEAN): {
                        lua_pushboolean(L, data.get<bool>());
                        break;
                    }
                    case (PropertyType::INTEGER): {
                        lua_pushinteger(L, data.get<int>());
                        break;
                    }
                    case (PropertyType::REAL): {
                        lua_pushnumber(L, data.get<double>());
                        break;
                    }
                    case (PropertyType::STRING): {
                        lua_pushstring(L,data.get<std::string>().c_str());
                        break;
                    }
                    default: {
                        std::string value = data.get<std::string>();
                        if (value.empty()) {
                            lua_pushnil(L);
                        }
                        else {
                            lua_getglobal(L, ENGINE_ENV);
                            lua_getfield(L, -1, value.c_str());

                            // Assigns value from TE namespace if match is found
                            if (!lua_isnil(L, -1)) {
                                lua_remove(L, -2);
                            }
                            // Assigns string
                            else {
                                lua_pop(L, 2);
                                lua_pushstring(L, value.c_str());
                            }
                        }

                        break;
                    }
                }

                lua_setfield(L, -2, name.c_str());
            }
        lua_rawset(L, -3);

        lua_pushvalue(L, originalTableIndex);   // idx, table, idx
        lua_setfield(L, -2, "object_index");    // table, idx

        lua_pushstring(L, "__cpp_ptr");         // ptr, table, idx
        lua_pushlightuserdata(L, instance);     // ud, ptr, table, idx
        lua_rawset(L, -3);                      // table, idx

        luaL_setmetatable(L, "__te_object");

    return 1;
}

static int RoomInstanceCreate(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float depth = luaL_checknumber(L, 4);
    Object* original = lua_toclass<Object>(L, 5);

    ObjectId objectId = room->currentId++;
    std::unique_ptr<Object> o = std::make_unique<Object>(*original);
        lua_pushnewinstance(L, 5, objectId, o.get(), original);
    int tableIdx = luaL_ref(L, LUA_REGISTRYINDEX);
    
    o->hasTable = true;
    o->tableReference = tableIdx;

    Object* ptr = o.get();
    ptr->MyReference = { objectId, room->myId, ptr };
    room->addQueue.push_back(std::move(o));
    room->ids[objectId] = ptr;

    ptr->x = x;
    ptr->y = y;
    ptr->depth = depth;
    ptr->runScriptTimestep("create", 1);
    ptr->xPrevRender = ptr->xPrev = ptr->x;
    ptr->yPrevRender = ptr->yPrev = ptr->y;

        // Push the table index back
        lua_rawgeti(L, LUA_REGISTRYINDEX, tableIdx);
    return 1;
}

static int RoomSetRenderPosition(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);
    float x = lua_tonumber(L, 2);
    float y = lua_tonumber(L, 3);
    room->setView(x, y);

    return 0;
}

static int RoomInstanceListCreate(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);
    Object* BASEOBJECT = lua_toclass<Object>(L, 2);
    int count = 0;
    lua_newtable(L);
    for (auto& [k, i] : room->ids) {
        if (i->extends(BASEOBJECT)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, i->tableReference);
            count++;
            lua_rawseti(L, -2, count);
        }
    }
    return 1;
}

static int RoomInstanceCount(lua_State* L) {
    int c = 0;
    Room* room = lua_toclass<Room>(L, 1);
    Object* baseClass = lua_toclass<Object>(L, 2);
    Object* check = (lua_getfieldexists(L, 2, "__id")) ? baseClass->self : baseClass;
    for (auto& [k, i] : room->ids) {
        if (i->extends(check)) {
            ++c;
        }
    }
    lua_pushinteger(L, c);
    return 1;
}

static int RoomInstanceGet(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);
    Object* baseClass = lua_toclass<Object>(L, 2);
    Object* check = (lua_getfieldexists(L, 2, "__id")) ? baseClass->self : baseClass;
    for (auto& [k, i] : room->ids) {
        if (i->extends(check)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, i->tableReference);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

void Room::initializeLua(LuaState& lua, const std::filesystem::path &assets) {
    // Room create function
    lua_getglobal(lua, ENGINE_ENV);
        lua_pushcfunction(lua, L_ROOM_CREATE);
        lua_setfield(lua, -2, "room_create");
    lua_pop(lua, 1);

    // Create Room class type info
    luaL_newmetatable(lua, "__te_room");
        lua_pushcfunction(lua, RoomGet);                    lua_setfield(lua, -2, "__index");
        lua_pushcfunction(lua, RoomSet);                    lua_setfield(lua, -2, "__newindex");
        lua_pushcfunction(lua, RoomGarbageCollect);         lua_setfield(lua, -2, "__gc");
        lua_pushcfunction(lua, RoomStep);                   lua_setfield(lua, -2, "step");
        lua_pushcfunction(lua, RoomDraw);                   lua_setfield(lua, -2, "draw");
        lua_pushcfunction(lua, RoomInstanceCreate);         lua_setfield(lua, -2, "instance_create");
        lua_pushcfunction(lua, RoomInstanceGet);            lua_setfield(lua, -2, "instance_get");
        lua_pushcfunction(lua, RoomInstanceRect);           lua_setfield(lua, -2, "instance_rect");
        lua_pushcfunction(lua, RoomInstancesRect);          lua_setfield(lua, -2, "instances_rect");
        lua_pushcfunction(lua, RoomInstanceExists);         lua_setfield(lua, -2, "instance_exists");
        lua_pushcfunction(lua, RoomInstanceDestroy);        lua_setfield(lua, -2, "instance_destroy");
        lua_pushcfunction(lua, RoomInstanceListCreate);     lua_setfield(lua, -2, "instance_list_create");
        lua_pushcfunction(lua, RoomSetRenderPosition);      lua_setfield(lua, -2, "set_render_position");
        lua_pushcfunction(lua, RoomInstanceCount);          lua_setfield(lua, -2, "instance_count");
    lua_pop(lua, 1);

    // Create view class type info
    luaL_newmetatable(lua, "__te_view");
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);

            lua_pushnumber(lua, c->x);
            return 1;
        }); lua_setfield(lua, -2, "get_x");

        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);

            double newNum = luaL_checknumber(lua, 2);
            c->x = newNum;
            
            return 0;
        }); lua_setfield(lua, -2, "set_x");

        // Get Y
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);
            lua_pushnumber(lua, c->y);
            return 1;
        }); lua_setfield(lua, -2, "get_y");

        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);

            double newNum = luaL_checknumber(lua, 2);
            c->y = newNum;
            
            return 0;
        }); lua_setfield(lua, -2, "set_y");

        // Get X Previous
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);
            lua_pushnumber(lua, c->xPrev);
            return 1;
        }); lua_setfield(lua, -2, "get_x_previous");

        // Get Y Previous
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);
            lua_pushnumber(lua, c->yPrev);
            return 1;
        }); lua_setfield(lua, -2, "get_y_previous");

        // Teleport
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);
            float x = lua_tonumber(lua, 2);
            float y = lua_tonumber(lua, 3);
            c->teleport(x, y);
            return 0;
        }); lua_setfield(lua, -2, "teleport");

        // Get Width
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);

            lua_pushnumber(lua, c->width);
            return 1;
        }); lua_setfield(lua, -2, "get_width");

        // Get Height
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);

            lua_pushnumber(lua, c->height);
            return 1;
        }); lua_setfield(lua, -2, "get_height");

        // Get
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            const char* key = lua_tostring(lua, 2);
            if (strcmp(key, "stay_in_bounds") == 0) {
                lua_getfield(lua, 1, "__cpp_ptr");
                auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
                lua_pop(lua, 1);

                lua_pushboolean(lua, c->stayInBounds);
                return 1;
            }

            lua_getmetatable(lua, 1);   // -1: meta
            lua_pushvalue(lua, 2);      // -1: key, -2: meta
            lua_gettable(lua, -2);      // -1: value, -2: meta (key was popped)
            lua_remove(lua, -2);        // bye table

            return 1;

        }); lua_setfield(lua, -2, "__index");

        // Set
        lua_pushcfunction(lua, [](lua_State* lua) -> int {
            const char* key = lua_tostring(lua, 2);
            lua_getfield(lua, 1, "__cpp_ptr");
            auto c = static_cast<Room::Camera*>(lua_touserdata(lua, -1));
            lua_pop(lua, 1);

            if (strcmp(key, "stay_in_bounds") == 0) {
                c->stayInBounds = lua_toboolean(lua, 3);
                return 0;
            }
            return 0;
        }); lua_setfield(lua, -2, "__newindex");

    lua_pop(lua, 1);

    lua_getglobal(lua, ENGINE_ENV);
    for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "rooms")) {
        if (!it.is_regular_file() || it.path().extension() != ".bin") {
            continue;
        }

        std::filesystem::path p = it.path();
        std::string identifier = p.filename().replace_extension("").string();

        RoomReference& ref = Game::get().roomReferences[identifier];
        ref.name = identifier;
        ref.p = p;

        lua_pushlightuserdata(lua, &ref);
        lua_setfield(lua, -2, identifier.c_str());
    }
    lua_pop(lua, 1);
}

void Room::load(int roomIdx) {
    // TODO
    using namespace nlohmann;

    auto& game = Game::get();
    auto& objMgr = ObjectManager::get();
    auto& lua = game.L;

    camera.width = game.canvasWidth;
    camera.height = game.canvasHeight;

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
            bg->vectorPos = instances.size() - 1;
            instances.push_back(std::move(bg));
        }

        else if (type == "tiles") {
            std::unique_ptr<Tilemap> map = std::make_unique<Tilemap>(lua);
            map->name = name;
            map->depth = depth;
            map->visible = visible;

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
            map->vectorPos = instances.size() - 1;
            instances.push_back(std::move(map));
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
                            lua_pushnewinstance(L, objIdx, objectId, ptr, original);
                        int tableIdx = luaL_ref(L, LUA_REGISTRYINDEX); // idx
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

// Room ->      "Create"
// Instances -> "Create"
// Room ->      Creation Code
// Room ->      "Room Start"
// Instances -> "Room Start"
// TODO
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

    camera.xPrev = camera.x;
    camera.yPrev = camera.y;

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

void Background::draw(Room* room, float alpha) {
    float cx = room->renderCameraX;
    float cy = room->renderCameraY;

    float x = cx - 1;
    float y = cy - 1;

    sf::Shader* shader = Game::get().currentShader;
    if (spriteIndex && spriteIndex->sprite) {
        sf::Sprite* spr = spriteIndex->sprite.get();
        spr->setScale({ 1, 1 });
        spr->setOrigin({ 0, 0 });
        spr->setColor(color);
        spr->setRotation(sf::degrees(0));
        spr->setColor({ 255, 255, 255, 255 });
        float parallax = xspd;
        float parallaxY = yspd;
        float x = (cx * parallax) + this->x;
        float timesOver = floorf((cx * (1.0f - parallax)) / spriteIndex->width);
        x += (spriteIndex->width) * timesOver;

        float y = (cy * parallaxY) + this->y;
        timesOver = floorf((cy * (1.0f - parallaxY)) / spriteIndex->height);
        y += (spriteIndex->height) * timesOver;

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (!tiledY && j != 0) continue;
                spr->setPosition({ floorf(x) + (i * spriteIndex->width), floorf(y) + (j * spriteIndex->height) });
                Game::get().getRenderTarget()->draw(*spr, shader);
            }
        }
    }
    else {
        sf::RectangleShape rs({ room->camera.width + 2, room->camera.height + 2 });
        rs.setTexture(&GFX::whiteTexture);
        rs.setFillColor(color);
        rs.setPosition({ x, y });
        Game::get().getRenderTarget()->draw(rs, shader);
    }
}