#pragma once

#include <string>
#include "object.h"

class Tileset;
class Tilemap : public Object {
public:
    std::vector<unsigned int> tileData;
    int tileCountX, tileCountY;
    std::string name;
    Tileset* tileset;
    Tilemap(sol::state& lua) : Object(lua) {}
    void draw(Room* room, float alpha) override;
    int get(int x, int y);
    std::tuple<int, bool, bool, bool> getExt(int x, int y);
    void set(int x, int y, int value);
    void setExt(int x, int y, int value, bool mirror, bool flip, bool rotate);
    static void initializeLua(sol::state& lua);
};