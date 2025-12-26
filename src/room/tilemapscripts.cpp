#include "room.h"
#include "../gfx/tileset.h"
#include "../game.h"

// getters & setters

static int TilemapGetDepth(lua_State* L) {
    lua_pushinteger(L, lua_toclass<Tilemap>(L, 1)->depth);
    return 1;
}

static int TilemapSetDepth(lua_State* L) {
    lua_toclass<Tilemap>(L, 1)->depth = luaL_checknumber(L, 3);
    return 0;
}

static int TilemapGetVisible(lua_State* L) {
    lua_pushinteger(L, lua_toclass<Tilemap>(L, 1)->visible);
    return 1;
}

static int TilemapSetVisible(lua_State* L) {
    lua_toclass<Tilemap>(L, 1)->visible = luaL_checknumber(L, 3);
    return 0;
}

static const luaL_Reg tilemapIndexFields[] = {
    { "depth", TilemapGetDepth },
    { "visible", TilemapGetVisible },
    { NULL, NULL }
};

static const luaL_Reg tilemapNewIndexFields[] = {
    { "depth", TilemapSetDepth },
    { "visible", TilemapSetVisible },
    { NULL, NULL }
};

// funcs

static int TilemapIndex(lua_State* L) {
    const char* key = lua_tostring(L, 2);

    lua_getfield(L, LUA_REGISTRYINDEX, "__te_tilemap_getters");
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

static int TilemapNewIndex(lua_State* L) {
    const char* key = lua_tostring(L, 2);

    lua_getfield(L, LUA_REGISTRYINDEX, "__te_tilemap_setters");
    lua_getfield(L, -1, key);
    if (!lua_isnil(L, -1)) {
        lua_remove(L, -2);
        lua_tocfunction(L, -1)(L);
        return 0;
    }
    lua_pop(L, 2);


    if (strcmp(key, "draw") == 0) {
        lua_pushvalue(L, 2);    // k
        lua_pushvalue(L, 3);    // v
        lua_rawset(L, 1);       // -k,-v
        return 0;
    }

    return 0;
}

static int TilemapDrawVerticesExt(lua_State* L) {
    Tilemap* tilemap = lua_toclass<Tilemap>(L, 1);
    Room* room = lua_toclass<Room>(L, 2);
    float alpha = luaL_checknumber(L, 3);
    float x = luaL_checknumber(L, 4);
    float y = luaL_checknumber(L, 5);
    float w = luaL_checknumber(L, 6);
    float h = luaL_checknumber(L, 7);

    auto& game = Game::get();
    auto view = game.getRenderTarget()->getView();
    tilemap->drawVertices(room, alpha, x, y, w, h);

    return 0;
}

static int TilemapDrawVertices(lua_State* L) {
    Tilemap* tilemap = lua_toclass<Tilemap>(L, 1);
    Room* room = lua_toclass<Room>(L, 2);
    float alpha = luaL_checknumber(L, 3);

    auto& game = Game::get();
    auto view = game.getRenderTarget()->getView();
    tilemap->drawVertices(room, alpha, game.getCanvasPosX(), game.getCanvasPosY(), view.getSize().x, view.getSize().y);

    return 0;
}

static int TilemapSetTileset(lua_State* L) {
    Tilemap* tilemap = lua_toclass<Tilemap>(L, 1);
    Tileset* tileset = lua_toclass<Tileset>(L, 2);
    tilemap->tileset = tileset;
    return 0;
}

// room funcs
static int RoomTilemapsGet(lua_State* L) {
    auto room = lua_toclass<Room>(L, 1);

    int count = 0;

    lua_newtable(L);

    for (auto& tilemap : room->tilemaps) {
        if (tilemap->hasTable) {
            count++;
            lua_rawgeti(L, LUA_REGISTRYINDEX, tilemap->tableReference);
            lua_rawseti(L, -2, count);
        }
    }

    return 1;
}

static int RoomTilemapGet(lua_State* L) {
    auto room = lua_toclass<Room>(L, 1);
    auto str = lua_tostring(L, 2);

    for (auto& tilemap : room->tilemaps) {
        if (tilemap->hasTable && strcmp(tilemap->name.c_str(), str) == 0) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, tilemap->tableReference);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static const luaL_Reg tilemapMetaFunctions[] = {
    { "__index",        TilemapIndex },
    { "__newindex",     TilemapNewIndex },
    { "set_tileset",    TilemapSetTileset },
    { "draw_vertices",  TilemapDrawVertices },
    { "draw_vertices_ext",  TilemapDrawVerticesExt },
    { NULL, NULL }
};

static const luaL_Reg roomTilemapFunctions[] = {
    { "tilemap_get", RoomTilemapGet },
    { "tilemaps_get", RoomTilemapsGet },
    { NULL, NULL }
};

void TilemapInitializeLua(lua_State* L, const std::filesystem::path& assets) {
    luaL_getmetatable(L, "Room");
    luaL_setfuncs(L, roomTilemapFunctions, 0);
    lua_pop(L, 1);

    luaL_newmetatable(L, "Tilemap");
    luaL_setfuncs(L, tilemapMetaFunctions, 0);
    lua_pop(L, 1);

    lua_newtable(L);
    luaL_setfuncs(L, tilemapIndexFields, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, "__te_tilemap_getters");

    lua_newtable(L);
    luaL_setfuncs(L, tilemapNewIndexFields, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, "__te_tilemap_setters");
}