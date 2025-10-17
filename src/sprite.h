#pragma once

#include <unordered_map>
#include <SFML/Graphics.hpp>

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

    void drawOrigin(
        sf::RenderTarget& target,
        sf::Vector2f position,
        float frame,
        sf::Vector2f scale,
        sf::Vector2f origin,
        sf::Color color,
        float rotation) const {

        int frameCount = frames.size();
        int frameIndex = static_cast<int>(frame) % frameCount;

        sprite->setTextureRect({ { frames[frameIndex].frameX, frames[frameIndex].frameY }, { width, height } });
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
        float rotation = 0) const {

        int frameCount = frames.size();
        int frameIndex = static_cast<int>(frame) % frameCount;

        sprite->setTextureRect({ { frames[frameIndex].frameX, frames[frameIndex].frameY }, { width, height } });
        sprite->setPosition(position);
        sprite->setOrigin({ static_cast<float>(originX), static_cast<float>(originY) });
        sprite->setScale(scale);
        sprite->setColor(color);
        sprite->setRotation(sf::degrees(rotation));

        target.draw(*(sprite.get()));
    }
};

class SpriteManager {
public:
    std::unordered_map<std::string, SpriteIndex> sprites;
    static SpriteManager& get() {
        static SpriteManager sm;
        return sm;
    }
};