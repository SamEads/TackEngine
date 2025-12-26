#include "room.h"
#include "../game.h"

int PushNewInstance(lua_State* L, int originalTableIndex, ObjectId objectId, Object* instance, Object* pseudoclass) {
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

        luaL_setmetatable(L, "Object");

    return 1;
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

static int RoomStep(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);

    room->view.xPrev = room->view.x;
    room->view.yPrev = room->view.y;

    Game& game = Game::get();
    room->view.width = game.canvasWidth;
    room->view.height = game.canvasHeight;

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

static int RoomDraw(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);

    float alpha = luaL_checknumber(L, 2);

    room->drawables.clear();

    for (auto& i : room->instances) {
        if (i->visible && i->active) {
            room->drawables.push_back(i.get());
        }
    }

    std::sort(room->drawables.begin(), room->drawables.end(), [](const auto a, const auto b) {
        return a->depth > b->depth;
    });

    for (auto& d : room->drawables) {
        if (!d->hasTable) continue;
        d->runScriptDraw("begin_draw", 1, alpha);
    }

    for (auto& d : room->drawables) {
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
        if (!d->hasTable) continue;
        d->runScriptDraw("end_draw", 1, alpha);
    }

    auto target = Game::get().getRenderTarget();
    target->setView(target->getDefaultView());

    for (auto& d : room->drawables) {
        if (!d->hasTable) continue;
        d->runScriptDraw("draw_gui", 1, alpha);
    }

    return 0;
}

static int RoomSet(lua_State* L) {
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

static int RoomCreate(lua_State* L) {
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
            auto view = &room->view;
            lua_pushlightuserdata(L, view);         // Light userdata, the camera is already being GC'd by Lua since the Room is Lua-owned
            lua_setfield(L, -2, "__cpp_ptr");       // This works fine since I do tables really weird and don't let Lua and C++ know everything about each others contexts..
            luaL_setmetatable(L, "RoomView");
        lua_setfield(L, -2, "view"); // Set table as "view" field

        luaL_setmetatable(lua, "Room"); // -1: room table

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

    lua_getfield(L, 2, "__id");
    bool instance = !lua_isnil(L, -1);

    // Object instance
    if (instance) {
        int id = lua_tonumber(L, -1);
        lua_pop(L, 1);

        auto idPos = room->ids.find(id);
        if (idPos != room->ids.end()) {
            auto object = idPos->second;
            object->runScriptTimestep("destroy", 1);
            room->deleteQueue.push_back(object->vectorPos);
            room->ids.erase(object->MyReference.id);
        }

        return 0;
    }

    lua_pop(L, 1);

    if (Object* original = lua_testclass<Object>(L, 2, "Object")) {
        for (auto& [k, i] : room->ids) {
            if (i->extends(original)) {
                ObjectId id = i->MyReference.id;
                auto idPos = room->ids.find(id);
                if (idPos != room->ids.end()) {
                    i->runScriptTimestep("destroy", 1);
                    room->deleteQueue.push_back(i->vectorPos);
                    room->ids.erase(id);
                }
            }
        }
        return 0;
    }
    
    if (Background* background = lua_testclass<Background>(L, 2, "Background")) {
        room->deleteQueue.push_back(background->vectorPos);
        auto it = std::find(room->backgrounds.begin(), room->backgrounds.end(), background);
        if (it != room->backgrounds.end()) {
            room->backgrounds.erase(it);
        }
        return 0;
    }

    if (Tilemap* tilemap = lua_testclass<Tilemap>(L, 2, "Tilemap")) {
        room->deleteQueue.push_back(tilemap->vectorPos);
        auto it = std::find(room->tilemaps.begin(), room->tilemaps.end(), tilemap);
        if (it != room->tilemaps.end()) {
            room->tilemaps.erase(it);
        }
        return 0;
    }

    return 0;
}

static int RoomInstanceCreate(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float depth = luaL_checknumber(L, 4);
    Object* original = lua_toclass<Object>(L, 5);

    ObjectId objectId = room->currentId++;
    std::unique_ptr<Object> o = std::make_unique<Object>(*original);
        PushNewInstance(L, 5, objectId, o.get(), original);
    int tableIdx = lua_reference(L, "instance");
    
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

/*
static int RoomSetRenderPosition(lua_State* L) {
    Room* room = lua_toclass<Room>(L, 1);
    float cx = lua_tonumber(L, 2);
    float cy = lua_tonumber(L, 3);

    auto target = Game::get().getRenderTarget();
    auto targetSize = target->getSize();
    float targetWidth = targetSize.x;
    float targetHeight = targetSize.y;

    sf::View view(sf::FloatRect { { 0.0f, 0.0f }, { targetWidth, targetHeight } });

    view.setCenter({ cx + targetWidth / 2.0f, cy + targetHeight / 2.0f });
    target->setView(view);

    room->renderCameraX = cx;
    room->renderCameraY = cy;

    return 0;
}
*/

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

static const luaL_Reg roomFunctions[] = {
    { "__index",                RoomGet },
    { "__newindex",             RoomSet },
    { "step",                   RoomStep },
    { "draw",                   RoomDraw },
    { "instance_create",        RoomInstanceCreate },
    { "instance_get",           RoomInstanceGet },
    { "instance_rect",          RoomInstanceRect },
    { "instances_rect",         RoomInstancesRect },
    { "instance_exists",        RoomInstanceExists },
    { "instance_destroy",       RoomInstanceDestroy },
    { "instance_list_create",   RoomInstanceListCreate },
    { "instance_count",         RoomInstanceCount },
    { NULL, NULL }
};

void RoomInitializeLua(lua_State* L, const std::filesystem::path& assets) {
    // TE ENVIRONMENT ADDITIONS (ROOM_CREATE & ALL ROOMS)
    lua_getglobal(L, ENGINE_ENV);
        lua_pushcfunction(L, RoomCreate);
        lua_setfield(L, -2, "room_create");
        for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "rooms")) {
            if (!it.is_regular_file() || it.path().extension() != ".bin") {
                continue;
            }

            std::filesystem::path p = it.path();
            std::string identifier = p.filename().replace_extension("").string();

            RoomReference& ref = Game::get().roomReferences[identifier];
            ref.name = identifier;
            ref.p = p;

            lua_pushlightuserdata(L, &ref);
            lua_setfield(L, -2, identifier.c_str());
        }
    lua_pop(L, 1);

    // ROOM METATABLE
    luaL_newmetatable(L, "Room");
    luaL_setfuncs(L, roomFunctions, 0);
    lua_pop(L, 1);
}