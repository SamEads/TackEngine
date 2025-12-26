#pragma once

#include <string>
#include "object/object.h"

class Tileset;
class Tilemap : public Object {
public:
    std::vector<unsigned int> tileData;
    int tileCountX, tileCountY;
    std::string name;
    Tileset* tileset;
    Tilemap(LuaState L) : Object(L) {}
    void draw(Room* room, float alpha) override;
    void drawVertices(Room* room, float alpha, float x, float y, float w, float h);
    int get(int x, int y);
    std::tuple<int, bool, bool, bool> getExt(int x, int y);
    void set(int x, int y, int value);
    void setExt(int x, int y, int value, bool mirror, bool flip, bool rotate);
};