extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <filesystem>

#ifndef LUAINC_H
#define LUAINC_H
#include <iostream>
struct LuaState;
struct LuaState {
    lua_State* l;
    int stack = 0;
    operator lua_State*() const { return l; }

    static LuaState& get(lua_State* l) {
        int type = lua_getfield(l, LUA_REGISTRYINDEX, "__cppstate");
        LuaState& lua = *(LuaState*)lua_touserdata(l, -1);
        lua_pop(l, 1);
        return lua;
    }

    // Stack +1
    void pushCFunction(lua_CFunction fn) {
        lua_pushcfunction(l, fn);
        stack++;
    }

    // Slots start at 1
    std::string getArgString(int slot) {
        const char* key = luaL_checkstring(l, slot);
        return key;
    }

    // Slots start at 1
    const char* getArgCString(int slot) {
        const char* key = luaL_checkstring(l, slot);
        return key;
    }

    // Slots start at 1
    double getArgDouble(int slot) {
        return luaL_checknumber(l, slot);
    }

    // Stack =
    template <typename T>
    T* getArgUserData(int slot, const std::string& tname) {
        void* ud = luaL_checkudata(l, slot, tname.c_str());
        return static_cast<T*>(ud);
    }

    // Stack =
    // Puts function on -1
    void setFunction(const std::string& str, lua_CFunction value) {
        pushCFunction(value);
        setField(-2, str.c_str());
    }

    // Stack -1
    void addTableToTable(const std::string& str) {
        setField(-2, str.c_str());
    }

    void pushString(const std::string& str) {
        lua_pushstring(l, str.c_str());
        stack++;
    }

    void pushDouble(double n) {
        lua_pushnumber(l, n);
        stack++;
    }

    int popResultAndReturn() {
        int st = stack;
        stack = 0;
        return st;
    }

    // Stack --
    void setTableCFunction(const std::string& str, lua_CFunction value) {
        pushString(str.c_str());
        pushCFunction(value);
        
        lua_settable(l, -3);
        stack -= 2;
    }

    void setTableDouble(const std::string& str, double value) {
        pushString(str.c_str());
        pushDouble(value);

        lua_settable(l, -3);
        stack -= 2;
    }

    // Returns the address of the block of memory casted to pointer T
    // Stack +1
    template <typename T>
    void* newUserdata() {
        void* n = lua_newuserdata(l, sizeof(T));
        stack++;
        return n;
    }

    // Stack -- (gets and sets)
    void setMetatable(const std::string& str) {
        luaL_setmetatable(l, str.c_str());
    }

    void doString(const std::string& str) {
        luaL_dostring(l, str.c_str());
    }

    // Stack +1
    void pushGlobal(const std::string& str) {
        lua_getglobal(l, str.c_str());
        stack++;
    }

    // Stack -1
    void setGlobal(const std::string& glob) {
        lua_setglobal(l, glob.c_str());
        stack--;
    }

    // Stack +1
    void getField(int idx, const std::string& f) {
        lua_getfield(l, idx, f.c_str());
        stack++;
    }

    // Stack -1
    void setField(int index, const std::string& str) {
        lua_setfield(l, index, str.c_str());
        stack--;
    }

    void pop(int amt) {
        lua_pop(l, amt);
        stack -= amt;
    }
    
    void newTable() {
        lua_newtable(l);
        stack++;
    }

    class LuaObject {

    };

    // Stack +1
    void newMetatable(const std::string& id) {
        luaL_newmetatable(l, id.c_str());
        stack++;
    }

    void checkpoint() {
        printf("[CHECKPOINT STACK]: %d\n", stack);
    }
    struct LuaData {
        LuaState* ls;
        LuaData(LuaState* ls) {
            this->ls = ls;
        }

        std::string globalToString(const std::string& s) {
            ls->pushGlobal(s);
            std::string res = luaL_tolstring(ls->l, -1, NULL);
            ls->pop(1);
            return res;
        }
    };
    LuaData D = LuaData(this);
};

inline bool LuaScript(LuaState& L, const std::filesystem::path& p) {
    return true;
}

#endif