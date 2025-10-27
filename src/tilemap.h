#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include "drawable.h"

class Tileset;
class Tilemap : public Drawable {
public:
    sf::VertexArray va;
    std::vector<unsigned int> tileData;
    int tileCountX, tileCountY;
    std::string name;
    Tileset* tileset;
    void draw(Room* room, float alpha) override;
};