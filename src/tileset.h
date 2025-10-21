#pragma once

#include "sprite.h"

class Tileset {
public:
    int offsetX, offsetY;
    int separationX, separationY;
    int tileCount;
    int tileCountX, tileCountY;
    int tileWidth, tileHeight;
    SpriteIndex* spriteIndex;
};

class TilesetManager {
public:
    std::unordered_map<std::string, Tileset> tilesets;
    static TilesetManager& get() {
        static TilesetManager tsm;
        return tsm;
    }
    void initializeLua(sol::state& lua, const std::filesystem::path& path);
};