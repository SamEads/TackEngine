#include <fstream>
#include "sprite.h"
#include "vendor/json.hpp"
#include "game.h"
#include "mathhelper.h"

using namespace nlohmann;

void SpriteManager::initializeLua(sol::state& lua, const std::filesystem::path& assets) {
    lua.new_usertype<SpriteIndex>(
        "SpriteIndex", sol::no_constructor,
        "origin_x", sol::readonly(&SpriteIndex::originX),
        "origin_y", sol::readonly(&SpriteIndex::originY),
        "height", sol::readonly(&SpriteIndex::height),
        "width", sol::readonly(&SpriteIndex::width)
    );

    for (auto& it : std::filesystem::directory_iterator(assets / "sprites")) {
        if (!it.is_directory()) {
            continue;
        }

        std::filesystem::path p = it.path();
        std::string identifier = p.filename().string();

        std::ifstream i(p / "data.json");
        json j = json::parse(i);

        SpriteIndex& spr = sprites[identifier];

        spr.texture = sf::Texture(p / "frames.png");
        spr.sprite = std::make_unique<sf::Sprite>(spr.texture);
        spr.width = j["width"];
        spr.height = j["height"];
        spr.originX = j["origin_x"];
        spr.originY = j["origin_y"];

        spr.hitbox.position.x = j["bbox_left"];
        spr.hitbox.position.y = j["bbox_top"];
        spr.hitbox.size.x = j["bbox_right"].get<float>() - spr.hitbox.position.x + 1.0f;
        spr.hitbox.size.y = j["bbox_bottom"].get<float>() - spr.hitbox.position.y + 1.0f;

        // temp handling of frames:
        int frameCount = j["frames"];
        for (int i = 0; i < frameCount; ++i) {
            spr.frames.push_back({ i * spr.width, 0 });
        }

        lua[identifier] = &spr;
    }

    // GFX
    lua.create_named_table("gfx");

    lua["gfx"]["draw_rectangle"] = [&](float x1, float y1, float x2, float y2, sol::table color) {
        sf::RectangleShape rs({ x2 - x1, y2 - y1 });
        rs.setPosition({ x1, y1 });
        rs.setFillColor(MakeColor(color));
        Game::get().currentRenderer->draw(rs);
    };

    lua["gfx"]["draw_sprite"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y) {
        if (spriteIndex == nullptr) return;
        spriteIndex->draw(*Game::get().currentRenderer, { x, y }, imageIndex);
    };

    lua["gfx"]["draw_sprite_ext"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y, float xscale, float yscale, float rot, sol::table color) {
        if (spriteIndex == nullptr) return;
        spriteIndex->draw(*Game::get().currentRenderer, { x, y }, imageIndex, { xscale, yscale }, MakeColor(color), rot);
    };

    lua["gfx"]["draw_sprite_ext_origin"] = [&](SpriteIndex* spriteIndex, float imageIndex, float x, float y, float xscale, float yscale, float originX, float originY, float rot, sol::table color) {
        if (spriteIndex == nullptr) return;
        spriteIndex->drawOrigin(*Game::get().currentRenderer, { x - spriteIndex->originX + originX, y - spriteIndex->originY + originY }, imageIndex, { xscale, yscale }, { originX, originY }, MakeColor(color), rot);
    };
}