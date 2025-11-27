#include "font.h"
#include "util/mathhelper.h"
#include "game.h"

void FontManager::initializeLua(LuaState& L, std::filesystem::path assets) {
    // TODO
    /*
    sol::table engineEnv = lua["TE"];

    std::filesystem::path fontsDirectory = assets / "fonts";
    if (std::filesystem::exists(fontsDirectory) && std::filesystem::is_directory(fontsDirectory)) {
        for (auto it : std::filesystem::directory_iterator(fontsDirectory)) {
            if (it.is_regular_file()) {
                const auto& fontPath = it.path();
                std::string fontName = fontPath.filename().replace_extension("").string();
                auto& font = fonts[fontName];
                font.isSpriteFont = false;
                font.fontIndex = sf::Font(fontPath);
                font.fontIndex.setSmooth(false);
                engineEnv[fontName] = &fonts[fontName];
            }
        }
    }

    engineEnv["gfx"]["create_font_from_sprite"] = [&](SpriteIndex* spriteIndex, const std::string& order) {
        Font font = {};
        font.spriteIndex = spriteIndex;
        font.isSpriteFont = true;
        for (int i = 0; i < order.length(); ++i) {
            font.charMap[order[i]] = i;
        }
        return font;
    };

    engineEnv["gfx"]["draw_font"] = [&](float x, float y, Font* font, int size, int spacing, const std::string& string, sol::table color) {
        if (font->isSpriteFont) {
            auto& spriteIndex = font->spriteIndex;
            sf::RenderTarget& currentRenderer = *Game::get().getRenderTarget();

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
        }
        else {
            sf::Text t(font->fontIndex, string, size);
            t.setFillColor(MakeColor(color));
            t.setPosition({ x, y });
            t.setLetterSpacing(spacing);
            Game::get().getRenderTarget()->draw(t);
        }
    };
    */
}