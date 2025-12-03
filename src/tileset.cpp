#include <fstream>
#include "tileset.h"
#include "vendor/json.hpp"

void TilesetManager::initializeLua(LuaState& L, const std::filesystem::path& assets) {
    // TODO
    TilesetManager& tsMgr = TilesetManager::get();

    lua_getglobal(L, ENGINE_ENV);
    for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "tilesets")) {
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
        ts.tileCount = j["tile_count"];
        ts.tileWidth = j["tile_width"];
        ts.tileHeight = j["tile_height"];
        ts.padding = 2;
        sf::Image src;
        std::string n = j["name"];
        GFX::sprites[j["sprite"]] = {};
        bool loaded = src.loadFromFile(assets / "managed" / "sprites" / j["sprite"].get<std::string>() / "frames.png");
        ts.tileCountX = (src.getSize().x - ts.offsetX + ts.separationX) / (ts.tileWidth + ts.separationX);
        ts.tileCountY = (src.getSize().y - ts.offsetY + ts.separationY) / (ts.tileHeight + ts.separationY);
        ts.tex = GFX::CreatePaddedTexture(
            src,
            ts.tileWidth,
            ts.tileHeight,
            ts.tileCountX,
            ts.tileCountY,
            ts.padding,
            ts.offsetX,
            ts.offsetY,
            ts.separationX,
            ts.separationY,
            nullptr
        );
        tsMgr.tilesets[identifier] = ts;
        lua_pushlightuserdata(L, &tsMgr.tilesets[identifier]);
        lua_setfield(L, -2, identifier.c_str());
    }
    lua_pop(L, 1);
}