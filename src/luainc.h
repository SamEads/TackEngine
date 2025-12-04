extern "C" {
#ifdef USE_LUA_JIT
#include <luajit.h>
#endif
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#define ENGINE_ENV "TE"

#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <filesystem>
#include <iostream>

#ifndef LUAINC_H
#define LUAINC_H

#ifdef USE_LUA_JIT

inline int lua_absindex(lua_State* L, int idx) {
    if (idx > 0 || idx <= LUA_REGISTRYINDEX) return idx;
    return lua_gettop(L) + idx + 1;
}

inline int lua_geti(lua_State* L, int idx, lua_Integer n) {
    int abs = lua_absindex(L, idx);
    lua_pushinteger(L, n);
    lua_gettable(L, abs);
    return lua_type(L, -1);
}

#define lua_rawlen lua_objlen

#endif

extern int refcount;
extern int refBaseline;
extern std::unordered_map<std::string, int> n;
#include <unordered_map>

static void printanalytics() {
    std::cout << "Refs: " << refcount;
    if (refcount == refBaseline) {
        std::cout << " (baseline)\n";
    }
    else {
        std::cout << "\n";
    }
    for (auto& [k,v] : n) {
        std::cout << "\t" << k << ": " << v << "\n";
    }
}

inline int lua_reference(lua_State* L, const std::string& shorthand) {
    n[shorthand]++;
    refcount++;
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

inline void lua_unreference(lua_State* L, int ref, const std::string& shorthand) {
    n[shorthand]--;
    refcount--;
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

inline int lua_lazycall(lua_State* L, int argCount, int retCount) {
    int res = lua_pcall(L, argCount, retCount, 0); // -1 TE
    if (res != LUA_OK) {
        luaL_traceback(L, L, lua_tostring(L, -1), 1);
        const char* tb = lua_tostring(L, -1);
        std::cout << tb << "\n";
        lua_pop(L, 1);
    }
    return res;
}

inline double lua_tonumbertable(lua_State* L, int idx, lua_Integer n) {
    lua_rawgeti(L, idx, n);
    double num = static_cast<double>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return num;
}

inline sf::Color lua_tocolor(lua_State* L, int idx) {
    lua_rawgeti(L, idx, 1); uint8_t r = static_cast<std::uint8_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
    lua_rawgeti(L, idx, 2); uint8_t g = static_cast<std::uint8_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
    lua_rawgeti(L, idx, 3); uint8_t b = static_cast<std::uint8_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
    lua_rawgeti(L, idx, 4); uint8_t a = static_cast<std::uint8_t>(lua_tonumber(L, -1)); lua_pop(L, 1);
    return { r, g, b, a };
}

template <typename T>
T* lua_toclass(lua_State* L, int idx) {
    lua_getfield(L, idx, "__cpp_ptr");
    int type = lua_type(L, -1);
        if (type == LUA_TNIL) {
            lua_pop(L, 1);
            return nullptr;
        }
        T* item = static_cast<T*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return item;
}

template <typename T>
T* lua_testclass(lua_State* L, int idx, const char* mt) {
    lua_getmetatable(L, idx);     // -1: obj mt
    luaL_getmetatable(L, mt);   // -1: registry mt, obj mt
    int equal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    if (!equal) {
        return nullptr;
    }
    lua_getfield(L, idx, "__cpp_ptr");
    int type = lua_type(L, -1);
        if (type == LUA_TNIL) {
            lua_pop(L, 1);
            return nullptr;
        }
        T* item = static_cast<T*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return item;
}

template <typename T>
bool lua_isclass(lua_State* L, int idx, const char* mt) {
    lua_getmetatable(L, idx);     // -1: obj mt
    luaL_getmetatable(L, mt);   // -1: registry mt, obj mt
    int equal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    if (!equal) {
        return false;
    }
    lua_getfield(L, idx, "__cpp_ptr");
    int type = lua_type(L, -1);
        if (type == LUA_TNIL) {
            lua_pop(L, 1);
            return false;
        }
    lua_pop(L, 1);
    return true;
}

template <typename T>
T* lua_toclassfromref(lua_State* L, int idx) {
    lua_getfield(L, idx, "__cpp_ptr");
    int type = lua_type(L, -1);
        if (type == LUA_TNIL) {
            lua_pop(L, 1);
            return nullptr;
        }
        T* item = static_cast<T*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return item;
}

inline bool lua_getfieldexists(lua_State* L, int idx, const char* str) {
    lua_getfield(L, idx, str);
    int type = lua_type(L, -1);
    lua_pop(L, 1);
    return type != LUA_TNIL;
}

struct LuaState {
    lua_State* l;
    
    operator lua_State*() const { return l; }

    static LuaState& get(lua_State* l) {
        lua_getfield(l, LUA_REGISTRYINDEX, "__cppstate");
        LuaState& lua = *(LuaState*)lua_touserdata(l, -1);
        lua_pop(l, 1);
        return lua;
    }
};

#endif