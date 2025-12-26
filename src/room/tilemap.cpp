#include "tilemap.h"
#include "../gfx/tileset.h"
#include "util/mathhelper.h"
#include "room/room.h"
#include "game.h"

void Tilemap::draw(Room* room, float alpha) {
    auto& game = Game::get();
    auto view = game.getRenderTarget()->getView();
    drawVertices(room, alpha, game.getCanvasPosX(), game.getCanvasPosY(), view.getSize().x, view.getSize().y);
}

void Tilemap::drawVertices(Room *room, float alpha, float cx, float cy, float w, float h) {
    int tileWidth = tileset->tileWidth;
    int tileHeight = tileset->tileHeight;

    if (tileWidth == 0 || tileHeight == 0) return;

    auto& game = Game::get();

    sf::RenderTarget* target = game.getRenderTarget();

    int thisCx =    std::max(0, (static_cast<int>(cx) / tileWidth));
    int thisCx2 =   thisCx + static_cast<int>(ceilf(w / (float)tileWidth)) + 1;
    int fullW =     std::min(thisCx2, tileCountX);
    
    int thisCy =    std::max(0, (static_cast<int>(cy) / tileHeight));
    int thisCy2 =   thisCy + static_cast<int>(ceilf(h / (float)tileHeight)) + 1;
    int fullH =     std::min(thisCy2, tileCountY);

    int padding = tileset->padding;

    int totalTiles = tileData.size();
    sf::VertexArray vertices(sf::PrimitiveType::Triangles);
    
    for (int xx = thisCx; xx < fullW; ++xx) {
        for (int yy = thisCy; yy < fullH; ++yy) {
            int pos = xx + (yy * tileCountX);
            if (pos >= totalTiles || pos < 0) continue;

            // no tile here
            int tile = tileData[pos];
            int mask = (1 << 19) - 1;
            int tileId = (tile & mask);
            if (tileId == 0) continue;
            
            bool mirror =   (tile & (1 << 28));
            bool flip =     (tile & (1 << 29));
            bool rotate =   (tile & (1 << 30));

            int tileX = tileId % tileset->tileCountX;
            int tileY = tileId / tileset->tileCountX;

            float x = xx * (float)tileWidth;
            float y = yy * (float)tileHeight;

            int texX = padding + tileX * (tileWidth + 2 * padding);
            int texY = padding + tileY * (tileHeight + 2 * padding);

            sf::Vector2f positions[] = {
                { x, y },
                { x + tileWidth, y },
                { x + tileWidth, y + tileHeight },
                { x, y + tileHeight} 
            };
            
            sf::Vector2f texCoords[] = {
                { (float)texX, (float)texY },
                { (float)(texX + tileWidth), (float)texY },
                { (float)(texX + tileWidth), (float)(texY + tileHeight) },
                { (float)texX, (float)(texY + tileHeight) }
            };

            if (mirror) {
                std::swap(texCoords[0], texCoords[1]);
                std::swap(texCoords[2], texCoords[3]);
            }
            if (flip) {
                std::swap(texCoords[0], texCoords[3]);
                std::swap(texCoords[1], texCoords[2]);
            }
            if (rotate) {
                auto temp = texCoords[0];
                texCoords[0] = texCoords[3];
                texCoords[3] = texCoords[2];
                texCoords[2] = texCoords[1];
                texCoords[1] = temp;
            }

            vertices.append(sf::Vertex{positions[0], sf::Color::White, texCoords[0]});
            vertices.append(sf::Vertex{positions[1], sf::Color::White, texCoords[1]});
            vertices.append(sf::Vertex{positions[2], sf::Color::White, texCoords[2]});
            
            vertices.append(sf::Vertex{positions[0], sf::Color::White, texCoords[0]});
            vertices.append(sf::Vertex{positions[2], sf::Color::White, texCoords[2]});
            vertices.append(sf::Vertex{positions[3], sf::Color::White, texCoords[3]});
        }
    }

    sf::Shader* shader = Game::get().currentShader;

    sf::RenderStates states;
    states.texture = &tileset->tex;
    states.shader = shader;
    target->draw(vertices, states);
}

int Tilemap::get(int x, int y) {
    int pos = x + (y * tileCountX);
    if (pos >= 0 && pos < tileCountX * tileCountY) {
        auto tile = tileData[pos];
        return tile;
    }

    return 0;
}

std::tuple<int, bool, bool, bool> Tilemap::getExt(int x, int y) {
    int pos = x + (y * tileCountX);
    if (pos >= 0 && pos < tileCountX * tileCountY) {
        auto tile = tileData[pos];
        bool mirror = (tile & (1 << 28));
        bool flip = (tile & (1 << 29));
        bool rotate = (tile & (1 << 30));
        int mask = (1 << 19) - 1;
        tile = tile & mask;
        return { tile, mirror, flip, rotate };
    }
    return { 0, false, false, false };
}

void Tilemap::set(int x, int y, int value) {
    int pos = x + (y * tileCountX);
    if (pos >= 0 && pos < tileCountX * tileCountY) {
        tileData[pos] = value;
    }
}

void Tilemap::setExt(int x, int y, int value, bool mirror, bool flip, bool rotate) {
    int pos = x + (y * tileCountX);
    if (pos >= 0 && pos < tileCountX * tileCountY) {
        uint32_t finalValue = value;
        if (mirror) finalValue |= (1 << 28);
        if (flip)   finalValue |= (1 << 29);
        if (rotate) finalValue |= (1 << 30);
        tileData[pos] = finalValue;
    }
}
