#include "font.h"
#include "mathhelper.h"
#include "game.h"

void FontManager::initializeLua(sol::state& lua) {
    lua.create_named_table("font");
    lua["font"]["add"] = [&](const std::string fontName, SpriteIndex* spriteIndex, const std::string& order) {
        auto& fm = FontManager::get();
        auto& font = fm.fonts[fontName];
        font.spriteIndex = spriteIndex;
        for (int i = 0; i < order.length(); ++i) {
            font.charMap[order[i]] = i;
        }
        lua[fontName] = &fm.fonts[fontName];
    };

    lua["font"]["draw"] = [&](float x, float y, Font* font, int spacing, const std::string& string, sol::table color) {
        auto& spriteIndex = font->spriteIndex;
        sf::RenderTarget& currentRenderer = *Game::get().currentRenderer;

        float cx = 0, cy = 0;
        for (int i = 0; i < string.length(); ++i) {
            char c = string[i];
            if (c == '\n') {
                cx = 0;
                cy += spriteIndex->height;
                continue;
            }
            if (c != ' ') {
                spriteIndex->draw(currentRenderer, { x + cx, y + cy }, font->charMap[string[i]], { 1, 1 }, MakeColor(color));
            }
            cx += spriteIndex->width + spacing;
        }
    };
}