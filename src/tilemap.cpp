#include "tilemap.h"
#include "tileset.h"
#include "util/mathhelper.h"
#include "room.h"
#include "game.h"

void Tilemap::initializeLua(sol::state &lua) {
    lua.new_usertype<Tilemap>(
        "Tilemap", sol::no_constructor,
        "visible", &Tilemap::visible,
        "set", &Tilemap::set,
        "get", &Tilemap::get,
        "depth", &Tilemap::depth
    );
}

void Tilemap::draw(Room* room, float alpha) {
    int tileWidth = tileset->tileWidth;
    int tileHeight = tileset->tileHeight;

    float cx = lerp(room->cameraPrevX, room->cameraX, alpha);
    float cy = lerp(room->cameraPrevY, room->cameraY, alpha);

    int thisCx = std::max(0, (static_cast<int>(cx) / tileWidth) - 1);
    int thisCx2 = thisCx + static_cast<int>(ceilf(room->cameraWidth / (float)tileWidth)) + 2;
    int fullW = std::min(thisCx2, tileCountX);
    
    int thisCy = std::max(0, (static_cast<int>(cy) / tileHeight) - 1);
    int thisCy2 = thisCy + static_cast<int>(ceilf(room->cameraHeight / (float)tileHeight)) + 2;
    int fullH = std::min(thisCy2, tileCountY);

    int padding = tileset->padding;

    sf::Sprite s(tileset->tex);
    float halfWidth = tileWidth / 2;
    float halfHeight = tileHeight / 2;
    s.setScale({ 1, 1 });
    s.setOrigin({ halfWidth, halfHeight });
    s.setRotation(sf::degrees(0));
    s.setColor({ 255, 255, 255, 255 });
    auto target = Game::get().currentRenderer;

    static int timer = 0;
    timer++;
    int totalTiles = tileData.size();
    for (int xx = thisCx; xx < fullW; ++xx) {
        for (int yy = thisCy; yy < fullH; ++yy) {
            int pos = xx + (yy * tileCountX);
            if (pos >= totalTiles || pos < 0) {
                continue;
            }

            int tile = tileData[pos];
            int mask = (1 << 19) - 1;
            if ((tile & mask) == 0) {
                continue;
            }
            
            bool mirror = (tile & (1 << 28));
            bool flip = (tile & (1 << 29));
            bool rotate = (tile & (1 << 30));
            tile = tile & mask;

            int tileX = tile % tileset->tileCountX;
            int tileY = tile / tileset->tileCountX;

            float scaleX = (mirror) ? -1 : 1;
            float scaleY = (flip) ? -1 : 1;
            
            float x = (xx * (float)tileWidth) + halfWidth;
            float y = (yy * (float)tileHeight) + halfHeight;

            s.setScale({ scaleX, scaleY }); 
            if (rotate) {
                s.setRotation(sf::degrees(90));
            }

            int texX = padding + tileX * (tileWidth + 2 * padding);
            int texY = padding + tileY * (tileHeight + 2 * padding);

            s.setTextureRect(sf::IntRect(
                { texX, texY },
                { (int)tileWidth, (int)tileHeight }
            ));

            s.setPosition({ (xx * (float)tileWidth) + halfWidth, (yy * (float)tileHeight) + halfHeight });

            target->draw(s);

            if (rotate) {
                s.setRotation(sf::degrees(0));
            }
        }
    }
}

int Tilemap::get(int x, int y) {
    return 0;
}

void Tilemap::set(int x, int y, int value) {

}
