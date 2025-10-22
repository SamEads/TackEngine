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
        ts.tileCount = j["tile_count"];
        ts.tileWidth = j["tile_width"];
        ts.tileHeight = j["tile_height"];
        ts.padding = 2;
        sf::Image src;
        std::string n = j["name"];
        std::cout << n << "\n";
        bool loaded = src.loadFromFile(assets / "sprites" / j["sprite"].get<std::string>() / "frames.png");
        ts.tileCountX = (src.getSize().x - ts.offsetX + ts.separationX) / (ts.tileWidth + ts.separationX);
        ts.tileCountY = (src.getSize().y - ts.offsetY + ts.separationY) / (ts.tileHeight + ts.separationY);
        ts.tex = CreatePaddedTexture(
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

    }
}