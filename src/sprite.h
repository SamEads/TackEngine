#pragma once

#include <unordered_map>
#include <SFML/Graphics.hpp>
#include <sol/sol.hpp>

class SpriteIndex {
public:
    struct SpriteFrame {
        int frameX = 0;
        int frameY = 0;
        SpriteFrame(int x, int y) : frameX(x), frameY(y) {}
    };
    std::vector<SpriteFrame> frames;

    sf::FloatRect hitbox{};

    int originX = 0, originY = 0;
    int width = 0, height = 0;

    sf::Texture texture;
    std::unique_ptr<sf::Sprite> sprite;

    float getTexelWidth();

    float getTexelHeight();

    int getCount() {
        return frames.size();
    }

    sol::table getUVs();
    sol::table getTexelSize();

    void drawOrigin(
        sf::RenderTarget& target,
        sf::Vector2f position,
        float frame,
        sf::Vector2f scale,
        sf::Vector2f origin,
        sf::Color color,
        float rotation) const 
    {
        int frameCount = frames.size();
        int frameIndex = static_cast<int>(frame) % frameCount;

        int texX = frames[frameIndex].frameX;
        int texY = frames[frameIndex].frameY;
        sprite->setTextureRect({ { texX, texY }, { width, height } });

        sprite->setPosition(position);
        sprite->setOrigin(origin);
        sprite->setScale(scale);
        sprite->setColor(color);
        sprite->setRotation(sf::degrees(rotation));

        target.draw(*(sprite.get()));
    }

    void draw(
        sf::RenderTarget& target,
        sf::Vector2f position,
        float frame = 0,
        sf::Vector2f scale = { 1.0f, 1.0f },
        sf::Color color = sf::Color::White,
        float rotation = 0) const 
    {
        int frameCount = frames.size();
        int frameIndex = static_cast<int>(floorf(frame)) % frameCount;

        int texX = frames[frameIndex].frameX;
        int texY = frames[frameIndex].frameY;
        sprite->setTextureRect({ { texX, texY }, { width, height } });

        sprite->setPosition(position);
        sprite->setOrigin({ static_cast<float>(originX), static_cast<float>(originY) });
        sprite->setScale(scale);
        sprite->setColor(color);
        sprite->setRotation(sf::degrees(rotation));

        target.draw(*(sprite.get()));
    }

};

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
    std::vector<SpriteIndex::SpriteFrame>* outFrameCoords = nullptr
);


class SpriteManager {
public:
    sf::Texture whiteTexture;
    std::unordered_map<std::string, SpriteIndex> sprites;
    static SpriteManager& get() {
        static SpriteManager sm;
        return sm;
    }
    void initializeLua(sol::state& lua, const std::filesystem::path& assets);
};