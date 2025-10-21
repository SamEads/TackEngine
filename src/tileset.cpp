#include <fstream>
#include "tileset.h"
#include "vendor/json.hpp"

void TilesetManager::initializeLua(sol::state &lua, const std::filesystem::path& assets) {
    TilesetManager& tsMgr = TilesetManager::get();
    SpriteManager& sprMgr = SpriteManager::get();
    for (auto& it : std::filesystem::directory_iterator(assets / "tilesets")) {
        if (!it.is_regular_file()) {
            continue;
        }
        std::string identifier = it.path().filename().replace_extension("").string();
        std::ifstream i(it.path());
        nlohmann::json j = nlohmann::json::parse(i);

        Tileset ts = {};

        ts.offsetX = j["offset_x"];
        ts.offsetY = j["offset_y"];
        ts.separationX = j["separation_x"];
        ts.separationY = j["separation_y"];
        ts.spriteIndex = &sprMgr.sprites[j["sprite"]];
        ts.tileCount = j["tile_count"];
        ts.tileWidth = j["tile_width"];
        ts.tileHeight = j["tile_height"];
        ts.tileCountX = ts.spriteIndex->width / ts.tileWidth;
        ts.tileCountY = ts.spriteIndex->height / ts.tileHeight;

        tsMgr.tilesets[identifier] = ts;
    }
}