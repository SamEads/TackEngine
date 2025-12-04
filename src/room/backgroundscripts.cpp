#include "room.h"

static int BackgroundGetDepth(lua_State* L) {
    lua_pushinteger(L, lua_toclass<Background>(L, 1)->depth);
    return 1;
}

static int BackgroundSetDepth(lua_State* L) {
    lua_toclass<Background>(L, 1)->depth = luaL_checknumber(L, 3);
    return 0;
}

static int BackgroundGetVisible(lua_State* L) {
    lua_pushinteger(L, lua_toclass<Background>(L, 1)->visible);
    return 1;
}

static int BackgroundSetVisible(lua_State* L) {
    lua_toclass<Background>(L, 1)->visible = luaL_checknumber(L, 3);
    return 0;
}

static int BackgroundSetColor(lua_State* L) {
    auto bg = lua_toclass<Background>(L, 1);
    bg->color = lua_tocolor(L, 2);
    return 0;
}

static int BackgroundGetColor(lua_State* L) {
    auto bg = lua_toclass<Background>(L, 1);
    lua_createtable(L, 4, 0);
    lua_pushinteger(L, bg->color.r); lua_rawseti(L, -2, 1);
    lua_pushinteger(L, bg->color.g); lua_rawseti(L, -2, 2);
    lua_pushinteger(L, bg->color.b); lua_rawseti(L, -2, 3);
    lua_pushinteger(L, bg->color.a); lua_rawseti(L, -2, 4);
    return 1;
}

static const luaL_Reg bgIndexFields[] = {
    { "depth", BackgroundGetDepth },
    { "visible", BackgroundGetVisible },
    { NULL, NULL }
};

static const luaL_Reg bgNewIndexFields[] = {
    { "depth", BackgroundSetDepth },
    { "visible", BackgroundSetVisible },
    { NULL, NULL }
};

static int BackgroundIndex(lua_State* L) {
    const char* key = lua_tostring(L, 2);

    lua_getfield(L, LUA_REGISTRYINDEX, "__te_bg_getters");
    lua_getfield(L, -1, key);
    if (!lua_isnil(L, -1)) {
        lua_remove(L, -2);  // getter table
        return lua_tocfunction(L, -1)(L);
    }
    lua_pop(L, 2);

    if (lua_getmetatable(L, 1)) {
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        if (!lua_isnil(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }
        lua_pop(L, 2);
    }

    lua_pushnil(L);
    return 1;
}

static int BackgroundNewIndex(lua_State* L) {
    const char* key = lua_tostring(L, 2);

    lua_getfield(L, LUA_REGISTRYINDEX, "__te_bg_setters");
    lua_getfield(L, -1, key);
    if (!lua_isnil(L, -1)) {
        lua_remove(L, -2);
        lua_tocfunction(L, -1)(L);
        return 0;
    }
    lua_pop(L, 2);

    return 0;
}

static int RoomBackgroundGet(lua_State* L) {
    auto caller = lua_toclass<Room>(L, 1);
    auto str = lua_tostring(L, 2);

    for (auto& bg : caller->backgrounds) {
        if (strcmp(bg->name.c_str(), str) == 0) {
            if (bg->hasTable) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, bg->tableReference);
                return 1;
            }
            lua_pushnil(L);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static const luaL_Reg bgMetaFunctions[] = {
    { "__index",        BackgroundIndex },
    { "__newindex",     BackgroundNewIndex },
    { "set_color",      BackgroundSetColor },
    { "get_color",      BackgroundGetColor },
    { NULL, NULL }
};

static const luaL_Reg roomBGFunctions[] = {
    { "background_get", RoomBackgroundGet },
    { NULL, NULL }
};

void BackgroundInitializeLua(lua_State* L, const std::filesystem::path& assets) {
    luaL_getmetatable(L, "Room");
    luaL_setfuncs(L, roomBGFunctions, 0);
    lua_pop(L, 1);

    luaL_newmetatable(L, "Background");
    luaL_setfuncs(L, bgMetaFunctions, 0);
    lua_pop(L, 1);

    lua_newtable(L);
    luaL_setfuncs(L, bgIndexFields, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, "__te_bg_getters");

    lua_newtable(L);
    luaL_setfuncs(L, bgNewIndexFields, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, "__te_bg_setters");
}