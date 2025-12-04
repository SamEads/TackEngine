#pragma once

#include <unordered_map>
#include <SFML/Graphics.hpp>
#include "luainc.h"

namespace GFX {
    class Sprite {
    public:
        struct Frame {
            int frameX = 0;
            int frameY = 0;
            Frame(int x, int y) : frameX(x), frameY(y) {}
        };

        int ref;
        int width = 0, height = 0;
        int originX = 0, originY = 0;
        sf::FloatRect hitbox {};
        std::vector<Frame> frames {};

        sf::Texture texture;
        std::unique_ptr<sf::Sprite> sprite;

        void drawOrigin(
            sf::RenderTarget& target,
            sf::Vector2f position,
            float frame,
            sf::Vector2f scale,
            sf::Vector2f origin,
            sf::Color color,
            float rotation) const;

        void draw(
            sf::RenderTarget& target,
            sf::Vector2f position,
            float frame = 0,
            sf::Vector2f scale = { 1.0f, 1.0f },
            sf::Color color = sf::Color::White,
            float rotation = 0) const;
    };

    extern sf::Texture whiteTexture;

    extern std::unordered_map<std::string, std::unique_ptr<GFX::Sprite>> sprites;

    sf::Texture CreatePaddedTexture(
        const sf::Image& source,
        unsigned int tileWidth,
        unsigned int tileHeight,
        unsigned int frameCountX,
        unsigned int frameCountY,
        unsigned int pad = 1,
        unsigned int offsetX = 0,
        unsigned int offsetY = 0,
        unsigned int separationX = 0,
        unsigned int separationY = 0,
        std::vector<GFX::Sprite::Frame>* outFrameCoords = nullptr);

    void initializeLua(LuaState& L, const std::filesystem::path& assets);
}