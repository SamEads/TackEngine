#include <iostream>
#include <fstream>
#include "vendor/json.hpp"
#include "object.h"
#include "game.h"
#include "room.h"

using namespace nlohmann;

bool Object::runScriptDraw(const std::string &script, int roomIdx, float alpha) {
    if (!hasTable) {
        return false;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, tableReference);
    int objIdx = lua_gettop(L);

    lua_getfield(L, objIdx, script.c_str());
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    
    lua_pushvalue(L, objIdx);
    lua_pushvalue(L, roomIdx);
    lua_pushnumber(L, alpha);
    lua_lazycall(L, 3, 0);

    lua_pop(L, 1); // pop object table
    return true;
}

bool Object::runScriptTimestep(const std::string &script, int roomIdx) {
    if (!hasTable) {
        return false;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, tableReference);
    int objIdx = lua_gettop(L);
    lua_getfield(L, objIdx, script.c_str());
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_pushvalue(L, objIdx);
    lua_pushvalue(L, roomIdx);
    lua_lazycall(L, 2, 0);
    lua_pop(L, 1); // pop object table
    return true;
}

std::vector<sf::Vector2f> Object::getPoints() const {
    if (imageAngle == 0) {
        sf::FloatRect rect = getRectangle();
        return std::vector<sf::Vector2f> {
            { rect.position.x, rect.position.y },
            { rect.position.x + rect.size.x, rect.position.y },
            { rect.position.x + rect.size.x, rect.position.y + rect.size.y },
            { rect.position.x, rect.position.y + rect.size.y }
        };
    }

    sf::FloatRect hb = {};
    if (maskIndex) {
        hb = maskIndex->hitbox;
    }
    else if (spriteIndex) {
        hb = spriteIndex->hitbox;
    }
    sf::Vector2f unscaledCorners[4] = {
        { hb.position.x, hb.position.y }, // top-left
        { hb.position.x + hb.size.x, hb.position.y }, // top-right
        { hb.position.x + hb.size.x, hb.position.y + hb.size.y }, // bottom-right
        { hb.position.x, hb.position.y + hb.size.y } // bottom-left
    };

    std::vector<sf::Vector2f> transformed;
    transformed.reserve(4);

    float rad = Deg2Rad(imageAngle);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    for (auto& corner : unscaledCorners) {
        sf::Vector2f scaled = { corner.x * xScale, corner.y * yScale };

        float originX = (spriteIndex) ? spriteIndex->originX : 0;
        float originY = (spriteIndex) ? spriteIndex->originY : 0;
        scaled.x -= originX * xScale;
        scaled.y -= originY * yScale;

        sf::Vector2f rotated = {
            scaled.x * cosA - scaled.y * sinA,
            scaled.x * sinA + scaled.y * cosA
        };

        rotated.x += x;
        rotated.y += y;

        transformed.push_back(rotated);
    }

    return transformed;
}

const bool Object::extends(Object* BaseObject) const {
    if (BaseObject == nullptr) return false;
    if (self == BaseObject) return true;
    if (parent == nullptr) return false;

    auto check = parent;
    while (true) {
        // found match
        if (check == BaseObject) {
            return true;
        }

        // continue upwards list search
        if (check->parent != nullptr) {
            check = check->parent;
        }
        else {
            break;
        }
    }
    return false;
}

void Object::draw(Room *room, float alpha) {
    if (!spriteIndex) {
        return;
    }
    
	float interpX = lerp(xPrevRender, x, alpha);
	float interpY = lerp(yPrevRender, y, alpha);
	spriteIndex->draw(*Game::get().getRenderTarget(), { interpX, interpY }, imageIndex, { xScale, yScale }, sf::Color::White, imageAngle);
}

void Object::beginDraw(Room *room, float alpha) { }
void Object::endDraw(Room *room, float alpha) { }
void Object::drawGui(Room *room, float alpha) { }

void ObjectManager::registerObject(const std::string &mapIdentifier, int luaRegistryRef, Object *innerUserdataPointer) {
    tilemapObjects[mapIdentifier] = { innerUserdataPointer, luaRegistryRef };
}

static PropertyType GuessType (const json& v) {
    if (v.is_number_integer())  return PropertyType::INTEGER;
    if (v.is_number())          return PropertyType::REAL;
    if (v.is_boolean())         return PropertyType::BOOLEAN;
    if (v.is_string())          return PropertyType::STRING;
    return PropertyType::STRING;
}

static void LoadDefaultProperties(lua_State* L, const json& props, Object* o) {
    std::vector<Object*> parentChain;
    for (auto p = o->parent; p != nullptr; p = p->parent) {
        parentChain.push_back(p);
    }
    for (auto it = parentChain.rbegin(); it != parentChain.rend(); ++it) {
        for (const auto& [key, value] : (*it)->baseProperties) {
            o->baseProperties.insert_or_assign(key, value);
        }
    }
    for (auto& p : props) {
        PropertyType type = PropertyType::NIL;
        if (p.contains("type")) {
            type = static_cast<PropertyType>(p.at("type").get<int>());
        }
        else {
            type = GuessType(p["value"]);
        }
        o->baseProperties.insert_or_assign(p["name"].get<std::string>(), std::pair { type, p["value"] });
    }
}
    
int ObjectCreateLua(lua_State* L, bool OLDARG) {
    int argcount = lua_gettop(L);

    lua_newtable(L); // this

    // If arg 1 is valid (should be another object)
    if (argcount > 0 && !lua_isnil(L, 1) && !lua_isstring(L, 1)) {
        // Set my metatable to the base object
        // TODO: Maybe push the metatable of the parent? idk
        luaL_setmetatable(L, "__te_object");

        // Set parent table to the parent fed in
        lua_pushvalue(L, 1);                // parent table, this
        lua_setfield(L, -2, "super");       // this(.__parent)

        // Get the parent __index too
        lua_pushvalue(L, 1);                // parent, this
        lua_setfield(L, -2, "__index");     // this(.__index = parent)
    }
    // Set metatable to __te_object and give self a table for values
    else {
        lua_pushvalue(L, -1);               // this, this
        lua_setfield(L, -2, "__index");     // this(.__index = this)

        luaL_setmetatable(L, "__te_object");
    }

    Object* o = new(lua_newuserdata(L, sizeof(Object))) Object(LuaState::get(L)); // ptr, this

    // Set parent
    if (argcount > 0 && lua_istable(L, 1)) {
        o->parent = lua_toclass<Object>(L, 1);
    }

    if (argcount > 0 && lua_isstring(L, 1)) {
        o->identifier = lua_tostring(L, 1);
    }

    // Set self
    o->self = o;
    
    lua_pushstring(L, "__cpp_ptr"); // -1: __cpp_ptr, -2: userdata ptr, -3: this
    lua_pushvalue(L, -2);           // -1: userdata ptr, -2: __cpp_ptr, -3: userdata ptr, -4: this
    lua_rawset(L, -4);              // -1: ptr, -2: this | t[k] = v, where t is at the index, v is top of stack (-1), k is just below top (-2)
    lua_pop(L, 1);
    
    // If arg 2 is a string
    if (argcount > 1 && lua_isstring(L, 2)) {
        const char* tilemapstr = lua_tostring(L, 2);

        lua_pushvalue(L, -1);                               // push the table again (this)
        int tableRef = luaL_ref(L, LUA_REGISTRYINDEX);      // store and get reference

        // Immediately open back up again and set self to self..
        lua_rawgeti(L, LUA_REGISTRYINDEX, tableRef);
        lua_pushstring(L, "object_index");
        lua_pushvalue(L, -2);
        lua_rawset(L, -3);
        lua_pop(L, 1);

        ObjectManager::get().registerObject(tilemapstr, tableRef, o);
        o->hasTable = true;
        o->tableReference = tableRef;
        o->identifier = tilemapstr;

        std::filesystem::path p = Game::get().assetsFolder / "managed" / "objects" / (std::string(tilemapstr) + ".json");

        if (std::filesystem::exists(p)) {
            std::ifstream i(p);
            json j = json::parse(i);
            if (!j["sprite"].is_null()) {
                auto it = GFX::sprites.find(j["sprite"]);
                if (it != GFX::sprites.end()) {
                    o->spriteIndex = it->second.get();
                }
            }
            o->visible = j["visible"];
            LoadDefaultProperties(L, j["properties"], o);
        }
    }

    return 1;
};

int ObjectIsA(lua_State* L) {
    if (lua_rawequal(L, 1, 2)) {
        lua_pushboolean(L, true);
        return 1;
    }

    lua_getfield(L, 1, "object_index");
    if (lua_rawequal(L, -1, 2)) {
        lua_pop(L, 1);
        lua_pushboolean(L, true);
        return 1;
    }

    lua_pushvalue(L, 1); // self
    while (true) {
        lua_getfield(L, -1, "super"); // parent, self
        if (lua_isnil(L, -1)) {
            lua_pop(L, 2); // -
            lua_pushboolean(L, 0); // is parent (false)
            return 1;
        }

        // Does equal
        if (lua_rawequal(L, -1, 2)) { // -1: self | 2: parent
            lua_pop(L, 2);
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_remove(L, -2);
    }

    return 0;
}

#define MAKEGETSET(val, type) \
static int set_##val(lua_State* L) { \
    Object* o = lua_toclass<Object>(L, 1); \
    o->val = lua_to##type(L, 3); \
    return 0; \
} \
static int get_##val(lua_State* L) { \
    Object* o = lua_toclass<Object>(L, 1); \
    lua_push##type(L, o->val); \
    return 1; \
}

MAKEGETSET(x, number)
MAKEGETSET(y, number)
MAKEGETSET(xPrev, number)
MAKEGETSET(yPrev, number)
MAKEGETSET(xPrevRender, number)
MAKEGETSET(yPrevRender, number)
MAKEGETSET(xspd, number)
MAKEGETSET(yspd, number)
MAKEGETSET(imageIndex, number)
MAKEGETSET(imageSpeed, number)
MAKEGETSET(imageAngle, number)
MAKEGETSET(depth, integer)
MAKEGETSET(incrementImageSpeed, boolean)
MAKEGETSET(active, boolean)
MAKEGETSET(visible, boolean)
MAKEGETSET(xScale, number)
MAKEGETSET(yScale, number)

void ObjectManager::initializeLua(LuaState& L, const std::filesystem::path &assets) {
    static const luaL_Reg getters[] = {
        { "x",                              get_x },
        { "y",                              get_y },
        { "x_previous",                     get_xPrev },
        { "y_previous",                     get_yPrev },
        { "x_previous_render",              get_xPrevRender },
        { "y_previous_render",              get_yPrevRender },
        { "hspeed",                         get_xspd },
        { "vspeed",                         get_yspd },
        { "image_index",                    get_imageIndex },
        { "image_angle",                    get_imageAngle },
        { "image_speed",                    get_imageSpeed },
        { "depth",                          get_depth },
        { "increment_image_speed",          get_incrementImageSpeed },
        { "active",                         get_active },
        { "visible",                        get_visible },
        { "image_xscale",                   get_xScale },
        { "image_yscale",                   get_yScale },
        { "sprite_index",                   [](lua_State* L) -> int {
                                                Object* o = lua_toclass<Object>(L, 1);
                                                if (!o->spriteIndex) {
                                                    lua_pushnil(L);
                                                    return 1;
                                                }
                                                lua_rawgeti(L, LUA_REGISTRYINDEX, o->spriteIndex->ref);
                                                return 1;
                                            } },
        { "mask_index",                     [](lua_State* L) -> int {
                                                Object* o = lua_toclass<Object>(L, 1);
                                                if (!o->maskIndex) {
                                                    lua_pushnil(L);
                                                    return 1;
                                                }
                                                lua_rawgeti(L, LUA_REGISTRYINDEX, o->maskIndex->ref);
                                                return 1;
                                            } },
        { NULL,                             NULL}
    };
    lua_newtable(L);
    luaL_setfuncs(L, getters, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, "__te_object_getters");

    static const luaL_Reg setters[] = {
        { "x",                              set_x },
        { "y",                              set_y },
        { "hspeed",                         set_xspd },
        { "vspeed",                         set_yspd },
        { "image_index",                    set_imageIndex },
        { "image_angle",                    set_imageAngle },
        { "image_speed",                    set_imageSpeed },
        { "depth",                          set_depth },
        { "increment_image_speed",          set_incrementImageSpeed },
        { "visible",                        set_visible },
        { "active",                         set_active },
        { "image_xscale",                   set_xScale },
        { "image_yscale",                   set_yScale },
        { "sprite_index",                   [](lua_State* L) -> int {
                                                Object* o = lua_toclass<Object>(L, 1);
                                                o->spriteIndex = lua_toclass<GFX::Sprite>(L, 3);
                                                return 0;
                                            } },
        { "mask_index",                     [](lua_State* L) -> int {
                                                Object* o = lua_toclass<Object>(L, 1);
                                                o->maskIndex = lua_toclass<GFX::Sprite>(L, 3);
                                                return 0;
                                            } },
        { NULL,                             NULL}
    };
    lua_newtable(L);
    luaL_setfuncs(L, setters, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, "__te_object_setters");

    lua_getglobal(L, ENGINE_ENV);
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ObjectCreateLua(L, true);
            return 1;
        });
        lua_setfield(L, -2, "object_create");

        luaL_newmetatable(L, "__te_object");
            // Get
            // 1: object, 2: key
            lua_pushcfunction(L, [](lua_State* L) -> int {
                const char* key = luaL_checkstring(L, 2);

                // GET FUNCS
                lua_getfield(L, LUA_REGISTRYINDEX, "__te_object_getters");
                lua_getfield(L, -1, key);
                if (!lua_isnil(L, -1)) {
                    lua_remove(L, -2);  // remove the getter table
                    return lua_tocfunction(L, -1)(L);
                }
                lua_pop(L, 2);

                // 1: TRY TO GET VALUE FROM SELF TABLE
                lua_pushvalue(L, 2);    // stack: key
                lua_rawget(L, 1);       // stack: value

                // Return self value
                if (!lua_isnil(L, -1)) { // checking value
                    return 1;
                }
                lua_pop(L, 1); // remove value

                // 2: TRY TO GET VALUE FROM OBJECT
                lua_pushstring(L, "object_index"); // str
                lua_rawget(L, 1);       // obj index(?)
                if (!lua_isnil(L, -1)) {
                    lua_pushvalue(L, 2); //     key, objindx
                    lua_rawget(L, -2);  //      ?val, objindx

                    if (!lua_isnil(L, -1)) {
                        lua_remove(L, -2);
                        return 1;
                    }
                }
                lua_pop(L, 1); // remove value

                // 3: TRY TO GET VALUE FROM SUPER(S)
                lua_pushstring(L, "super");
                lua_rawget(L, 1); // parent(?), key, obj
                while (!lua_isnil(L, -1)) {
                    lua_pushvalue(L, 2);        // key, parent, key, obj
                    lua_rawget(L, -2);          // value(?), parent, key, obj
                    
                    if (!lua_isnil(L, -1)) {
                        return 1;
                    }
                    
                    lua_pop(L, 1);                  // parent, key, obj
                    lua_getfield(L, -1, "super");   // nextparent(?), parent, key, obj
                    lua_remove(L, -2);              // nextparent(?), key, obj
                }
                lua_pop(L, 1); // key, obj

                // 4: TRY TO GET VALUE FROM MT
                if (lua_getmetatable(L, 1)) {
                    lua_pushvalue(L, 2);        // key, mt
                    lua_gettable(L, -2);        // val, mt
                    lua_remove(L, -2);          // val
                    return 1;
                }

                lua_pushnil(L);
                return 1;
            });
            lua_setfield(L, -2, "__index");

            // Set table[key] = value
            lua_pushcfunction(L, [](lua_State* L) -> int {
                const char* key = luaL_checkstring(L, 2);

                // SET FUNCS
                lua_getfield(L, LUA_REGISTRYINDEX, "__te_object_setters");
                lua_getfield(L, -1, key);
                if (!lua_isnil(L, -1)) {
                    lua_remove(L, -2);
                    int retargs = lua_tocfunction(L, -1)(L);
                    return retargs;
                }
                lua_pop(L, 2);

                lua_getfield(L, LUA_REGISTRYINDEX, "__te_object_getters");
                lua_getfield(L, -1, key);
                bool hasGetter = !lua_isnil(L, -1);
                lua_pop(L, 2);
                if (hasGetter) {
                    return luaL_error(L, "field '%s' is read-only", key);
                }

                lua_pushvalue(L, 2);    // key
                lua_pushvalue(L, 3);    // value, key
                lua_rawset(L, 1);       // =, obj.key = value

                return 0;
            });
            lua_setfield(L, -2, "__newindex");

            lua_pushcfunction(L, ObjectIsA);
            lua_setfield(L, -2, "is_a");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Object* o = lua_toclass<Object>(L, 1);
                float left = o->getBboxLeft();
                lua_pushnumber(L, left);
                return 1;
            });
            lua_setfield(L, -2, "bbox_left");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Object* o = lua_toclass<Object>(L, 1);
                lua_pushnumber(L, o->bboxTop());
                return 1;
            });
            lua_setfield(L, -2, "bbox_top");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Object* o = lua_toclass<Object>(L, 1);
                lua_pushnumber(L, o->bboxRight());
                return 1;
            });
            lua_setfield(L, -2, "bbox_right");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Object* o = lua_toclass<Object>(L, 1);
                lua_pushnumber(L, o->bboxBottom());
                return 1;
            });
            lua_setfield(L, -2, "bbox_bottom");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Object* o = lua_toclass<Object>(L, 1);
                o->y = o->yPrev = o->yPrevRender = luaL_checknumber(L, 2);
                return 0;
            });
            lua_setfield(L, -2, "force_y");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Object* o = lua_toclass<Object>(L, 1);
                o->x = o->xPrev = o->xPrevRender = luaL_checknumber(L, 2);
                return 0;
            });
            lua_setfield(L, -2, "force_x");

            lua_pushcfunction(L, [](lua_State* L) -> int {
                Object* o = lua_toclass<Object>(L, 1);
                o->x = o->xPrev = o->xPrevRender = luaL_checknumber(L, 2);
                o->y = o->yPrev = o->yPrevRender = luaL_checknumber(L, 3);
                return 0;
            });
            lua_setfield(L, -2, "force_position");
    lua_pop(L, 2);
}