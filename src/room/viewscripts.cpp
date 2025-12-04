#include "room.h"

static int ViewGet(lua_State* L) {
    const char* key = lua_tostring(L, 2);

    if (strcmp(key, "stay_in_bounds") == 0) {
        lua_pushboolean(L, lua_toclass<Room::View>(L, 1)->stayInBounds);
        return 1;
    }

    lua_getmetatable(L, 1);   // -1: meta
    lua_pushvalue(L, 2);      // -1: key, -2: meta
    lua_gettable(L, -2);      // -1: value, -2: meta (key was popped)
    lua_remove(L, -2);        // bye table

    return 1;
}

static int ViewSet(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    if (strcmp(key, "stay_in_bounds") == 0) {
        lua_toclass<Room::View>(L, 1)->stayInBounds = lua_toboolean(L, 3);
        return 0;
    }
    return 0;
}

static int ViewGetX(lua_State* L) {
    auto view = lua_toclass<Room::View>(L, 1);
    lua_pushnumber(L, view->x);
    return 1;
}

static int ViewGetY(lua_State* L) {
    auto view = lua_toclass<Room::View>(L, 1);
    lua_pushnumber(L, view->y);
    return 1;
}

static int ViewSetX(lua_State* L) {
    lua_toclass<Room::View>(L, 1)->setX(luaL_checknumber(L, 2));
    return 0;
}

static int ViewSetY(lua_State* L) {
    lua_toclass<Room::View>(L, 1)->setY(luaL_checknumber(L, 2));
    return 0;
}

static int ViewGetXPrevious(lua_State* L) {
    auto view = lua_toclass<Room::View>(L, 1);
    lua_pushnumber(L, view->xPrev);
    return 1;
}

static int ViewGetYPrevious(lua_State* L) {
    auto view = lua_toclass<Room::View>(L, 1);
    lua_pushnumber(L, view->yPrev);
    return 1;
}

static int ViewGetWidth(lua_State* L) {
    auto view = lua_toclass<Room::View>(L, 1);
    lua_pushnumber(L, view->width);
    return 1;
}

static int ViewGetHeight(lua_State* L) {
    auto view = lua_toclass<Room::View>(L, 1);
    lua_pushnumber(L, view->height);
    return 1;
}

static int ViewTeleport(lua_State* L) {
    float x = lua_tonumber(L, 2);
    float y = lua_tonumber(L, 3);
    lua_toclass<Room::View>(L, 1)->teleport(x, y);
    return 0;
}

static const luaL_Reg viewFunctions[] = {
    { "__index",           ViewGet },
    { "__newindex",        ViewSet },
    { "get_width",         ViewGetWidth },
    { "get_height",        ViewGetHeight },
    { "get_x",             ViewGetX },
    { "get_y",             ViewGetY },
    { "get_x_previous",    ViewGetXPrevious },
    { "get_y_previous",    ViewGetYPrevious },
    { "set_x",             ViewSetX },
    { "set_y",             ViewSetY },
    { "teleport",          ViewTeleport },
    { NULL, NULL }
};

void RoomViewInitializeLua(lua_State* L, const std::filesystem::path& assets) {
    // ROOM VIEW METATABLE
    luaL_newmetatable(L, "RoomView");
    luaL_setfuncs(L, viewFunctions, 0);
    lua_pop(L, 1);
}