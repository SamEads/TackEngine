#include <fstream>
#include "sprite.h"
#include "vendor/json.hpp"
#include "game.h"
#include "mathhelper.h"

using namespace nlohmann;

float SpriteIndex::getTexelWidth() {
    return 1;
}

float SpriteIndex::getTexelHeight() {
    return 1;
}

std::tuple<float, float, float, float> SpriteIndex::getUVs() {
    sf::Vector2u texSize = sprite->getTexture().getSize();
    // assuming frame 0 for now...
    float left = frames[0].frameX;
    float top = frames[0].frameY;
    float right = left + width;
    float bottom = top + height;
    left /= texSize.x;
    right /= texSize.x;
    top /= texSize.y;
    bottom /= texSize.y;
    return { left, top, right, bottom };
}

std::tuple<float, float> SpriteIndex::getTexelSize() {
    sf::Vector2u texSize = sprite->getTexture().getSize();
    return { 1.0f / texSize.x, 1.0f / texSize.y };
}

void SpriteManager::initializeLua(sol::state& lua, const std::filesystem::path& assets) {
    lua.new_usertype<SpriteIndex>(
        "SpriteIndex", sol::no_constructor,
        "origin_x", sol::readonly(&SpriteIndex::originX),
        "origin_y", sol::readonly(&SpriteIndex::originY),
        "height", sol::readonly(&SpriteIndex::height),
        "width", sol::readonly(&SpriteIndex::width),
        "get_uvs", &SpriteIndex::getUVs,
        "get_texel_size", &SpriteIndex::getTexelSize,
        "frame_count", sol::readonly_property(&SpriteIndex::getCount),
        "texel_width", sol::readonly_property(&SpriteIndex::getTexelWidth),
        "texel_height", sol::readonly_property(&SpriteIndex::getTexelHeight)
    );

    sf::Image white(sf::Vector2u { 1, 1 }, sf::Color::White);
    bool loadedWhite = whiteTexture.loadFromImage(white);
    if (!loadedWhite) {}

    auto paths = {
        assets / "sprites",
        assets / "managed" / "sprites",
    };
    for (const auto& path : paths) {
        for (auto& it : std::filesystem::directory_iterator(path)) {
            bool isPng = false;
            if (!it.is_directory()) {
                if (it.path().extension() != ".png") {
                    continue;
                }
                else {
                    isPng = true;
                }
            }
            int pad = 1;
            sf::Image src;
            int frameCount = 0;
            sf::Texture tex;
            std::vector<SpriteIndex::SpriteFrame> frameCoords;
            int frameCountX = -1, frameCountY = -1;
            std::string identifier = it.path().filename().replace_extension("").string();
            if (sprites.find(identifier) != sprites.end()) {
                continue;
            }
            SpriteIndex& spr = sprites[identifier];

            if (!isPng) {
                std::ifstream i(it.path() / "data.json");
                json j = json::parse(i);
                spr.width = j["size"][0].get<int>();
                spr.height = j["size"][1].get<int>();

                bool imageLoaded = src.loadFromFile(it.path() / "frames.png");

                int offByWidth = src.getSize().x / spr.width;
                int offByHeight = src.getSize().y / spr.height;
                frameCountX = offByWidth;
                frameCountY = offByHeight;

                std::vector<float> hitbox = j["hitbox"];
                spr.hitbox.position = { hitbox[0], hitbox[1] };
                spr.hitbox.size.x = hitbox[2] - hitbox[0] + 1.0f;
                spr.hitbox.size.y = hitbox[3] - hitbox[1] + 1.0f;

                spr.originX = j["origin"][0].get<int>();
                spr.originY = j["origin"][1].get<int>();

                tex = CreatePaddedTexture(src, spr.width, spr.height, frameCountX, frameCountY, pad, 0, 0, 0, 0, &frameCoords);
            }
            else {
                bool imageLoaded = src.loadFromFile(it.path().string());
                auto size = src.getSize();
                bool autoSize = true;
                bool autoHitbox = true;

                auto jPath = it.path().parent_path() / std::string(identifier + ".json");
                if (std::filesystem::exists(jPath)) {
                    std::ifstream i(jPath);
                    json j = json::parse(i);
                    if (j.contains("size") && j["size"].size() == 2) {
                        autoSize = false;
                        spr.width = j["size"][0].get<int>();
                        spr.height = j["size"][1].get<int>();

                        int offByWidth = src.getSize().x / spr.width;
                        int offByHeight = src.getSize().y / spr.height;
                        frameCountX = offByWidth;
                        frameCountY = offByHeight;
                    }
                    if (j.contains("hitbox") && j["hitbox"].size() == 4) {
                        autoHitbox = false;
                        std::vector<float> hitbox = j["hitbox"];
                        spr.hitbox.position = { hitbox[0], hitbox[1] };
                        spr.hitbox.size.x = hitbox[2] - hitbox[0] + 1.0f;
                        spr.hitbox.size.y = hitbox[3] - hitbox[1] + 1.0f;
                    }
                    if (j.contains("origin") && j["orig in"].size() == 2) {
                        spr.originX = j["origin"][0].get<int>();
                        spr.originY = j["origin"][1].get<int>();
                    }
                }
                
                if (autoSize) {
                    spr.width = spr.height = size.y;
                    frameCountX = size.x / size.y;
                }
                if (autoHitbox) {
                    spr.hitbox.position = { 0, 0 };
                    spr.hitbox.size = { static_cast<float>(spr.width), static_cast<float>(spr.height) };
                }

                if (frameCountY == -1) {
                    tex = CreatePaddedTexture(src, spr.width, spr.height, frameCountX, 1, pad, 0, 0, 0, 0, &frameCoords);
                }
                else {
                    tex = CreatePaddedTexture(src, spr.width, spr.height, frameCountX, frameCountY, pad, 0, 0, 0, 0, &frameCoords);
                }
            }

            spr.texture = tex;
            spr.sprite = std::make_unique<sf::Sprite>(spr.texture);
            spr.frames = frameCoords;
            lua[identifier] = &spr;
        }
    }

    // GFX
    sol::table gfx = lua.create_named_table("gfx");

    gfx["draw_rectangle"] = [&](float x1, float y1, float x2, float y2, sol::table color) {
        sf::RectangleShape rs({ x2 - x1, y2 - y1 });
        rs.setPosition({ x1, y1 });
        rs.setFillColor(MakeColor(color));
        rs.setTexture(&whiteTexture);
        rs.setTextureRect({ { 0, 0 }, { 1, 1 } });
        Game::get().currentRenderer->draw(rs);
    };

    gfx["draw_circle"] = [&](float x, float y, float r, sol::table color) {
        sf::CircleShape cs(r);
        cs.setPosition({ x, y });
        cs.setOrigin({ r, r });
        cs.setFillColor(MakeColor(color));
        cs.setTexture(&whiteTexture);
        cs.setTextureRect({ { 0, 0 }, { 1, 1 } });
        Game::get().currentRenderer->draw(cs);
    };

    gfx["draw_sprite"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y) {
        if (spriteIndex == nullptr) return;
        spriteIndex->draw(*Game::get().currentRenderer, { x, y }, imageIndex);
    };

    gfx["draw_sprite_ext"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y, float xscale, float yscale, float rot, sol::table color) {
        if (spriteIndex == nullptr) return;
        spriteIndex->draw(*Game::get().currentRenderer, { x, y }, imageIndex, { xscale, yscale }, MakeColor(color), rot);
    };

    gfx["draw_sprite_origin"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y, float xscale, float yscale, float originX, float originY, bool keepSpriteOriginPosition, float rot, sol::table color) {
        if (spriteIndex == nullptr) return;
        spriteIndex->drawOrigin(*Game::get().currentRenderer,
            (keepSpriteOriginPosition) ? sf::Vector2f(x - spriteIndex->originX + originX, y - spriteIndex->originY + originY) : sf::Vector2f(x, y),
            imageIndex, { xscale, yscale }, { originX, originY }, MakeColor(color), rot);
    };
}

sf::Texture CreatePaddedTexture(
    const sf::Image& source,
    unsigned int tileWidth,
    unsigned int tileHeight,
    unsigned int frameCountX,
    unsigned int frameCountY,
    unsigned int pad,
    unsigned int offsetX,
    unsigned int offsetY,
    unsigned int separationX,
    unsigned int separationY,
    std::vector<SpriteIndex::SpriteFrame>* outFrameCoords)
{
    unsigned int texWidth = frameCountX * (tileWidth + 2 * pad);
    unsigned int texHeight = frameCountY * (tileHeight + 2 * pad);
    sf::Image padded(sf::Vector2u(texWidth, texHeight), sf::Color::Transparent);

    for (unsigned int y = 0; y < frameCountY; ++y) {
        for (unsigned int x = 0; x < frameCountX; ++x) {
            unsigned int srcX = offsetX + x * (tileWidth + separationX);
            unsigned int srcY = offsetY + y * (tileHeight + separationY);

            unsigned int dstX = pad + x * (tileWidth + 2 * pad);
            unsigned int dstY = pad + y * (tileHeight + 2 * pad);

            bool copied = padded.copy(source, sf::Vector2u(dstX, dstY), sf::IntRect(sf::Vector2i(srcX, srcY), sf::Vector2i(tileWidth, tileHeight)), true);

            for (int i = 0; i < pad; ++i) {
                copied = padded.copy(source, sf::Vector2u(dstX - 1 - i, dstY), sf::IntRect(sf::Vector2i(srcX, srcY), sf::Vector2i(1, tileHeight)), true);
                copied = padded.copy(source, sf::Vector2u(dstX + tileWidth + i, dstY), sf::IntRect(sf::Vector2i(srcX + tileWidth - 1, srcY), sf::Vector2i(1, tileHeight)), true);
            }
            for (int i = 0; i < pad; ++i) {
                copied = padded.copy(source, sf::Vector2u(dstX, dstY - 1 - i), sf::IntRect(sf::Vector2i(srcX, srcY), sf::Vector2i(tileWidth, 1)), true);
                copied = padded.copy(source, sf::Vector2u(dstX, dstY + tileHeight + i), sf::IntRect(sf::Vector2i(srcX, srcY + tileHeight - 1), sf::Vector2i(tileWidth, 1)), true);
            }
            for (int i = 0; i < pad; ++i) {
                for (int j = 0; j < pad; ++j) {
                    // Top-left corner
                    copied = padded.copy(source, sf::Vector2u(dstX - 1 - i, dstY - 1 - j), sf::IntRect(sf::Vector2i(srcX, srcY), sf::Vector2i(1, 1)), true);
                    // Top-right corner
                    copied = padded.copy(source, sf::Vector2u(dstX + tileWidth + i, dstY - 1 - j), sf::IntRect(sf::Vector2i(srcX + tileWidth - 1, srcY), sf::Vector2i(1, 1)), true);
                    // Bottom-left corner
                    copied = padded.copy(source, sf::Vector2u(dstX - 1 - i, dstY + tileHeight + j), sf::IntRect(sf::Vector2i(srcX, srcY + tileHeight - 1), sf::Vector2i(1, 1)), true);
                    // Bottom-right corner
                    copied = padded.copy(source, sf::Vector2u(dstX + tileWidth + i, dstY + tileHeight + j), sf::IntRect(sf::Vector2i(srcX + tileWidth - 1, srcY + tileHeight - 1), sf::Vector2i(1, 1)), true);
                }
            }

            if (outFrameCoords)
                outFrameCoords->push_back(SpriteIndex::SpriteFrame{ static_cast<int>(dstX), static_cast<int>(dstY) });
        }
    }

    sf::Texture tex;
    bool loaded = tex.loadFromImage(padded);
    return tex;
}