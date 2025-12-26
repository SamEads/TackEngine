#include "font.h"
#include "util/mathhelper.h"
#include "game.h"

void FontManager::initializeLua(LuaState& L, std::filesystem::path assets) {
    lua_getglobal(L, ENGINE_ENV);

        std::filesystem::path fontsDirectory = assets / "fonts";
        if (std::filesystem::exists(fontsDirectory) && std::filesystem::is_directory(fontsDirectory)) {
            for (auto it : std::filesystem::directory_iterator(fontsDirectory)) {
                if (it.is_regular_file()) {
                    const auto& fontPath = it.path();
                    std::string fontName = fontPath.filename().replace_extension("").string();
                    auto* fontPtr = &fonts[fontName];
                    fontPtr->isSpriteFont = false;
                    fontPtr->fontIndex = sf::Font(fontPath);
                    fontPtr->fontIndex.setSmooth(false);

                    lua_pushlightuserdata(L, fontPtr);
                    lua_setfield(L, -2, fontName.c_str());
                }
            }
        }

        lua_getfield(L, -1, "gfx");
            // create_font_from_sprite
            // gfx::sprite sprite, string order
            lua_pushcfunction(L, [](lua_State* L) -> int {
                GFX::Sprite* spriteIndex = lua_toclassfromref<GFX::Sprite>(L, 1);
                std::string order = std::string(lua_tostring(L, 2));
                Font* font = new(lua_newuserdata(L, sizeof(Font))) Font();
                    font->spriteIndex = spriteIndex;
                    font->isSpriteFont = true;
                    for (int i = 0; i < order.length(); ++i) {
                        font->charMap[order[i]] = i;
                    }
                return 1;
            });
            lua_setfield(L, -2, "create_font_from_sprite");

            // draw_font
            // float x, float y, Font* font, int size, int spacing, const std::string& string, color
            lua_pushcfunction(L, [](lua_State* L) -> int {
                float x = lua_tonumber(L, 1);
                float y = lua_tonumber(L, 2);

                Font* font = static_cast<Font*>(lua_touserdata(L, 3));
                
                int size = lua_tointeger(L, 4);
                int spacing = lua_tointeger(L, 5);
                std::string string = std::string(lua_tostring(L, 6));

                sf::Color color = lua_tocolor(L, 7);

                if (font->isSpriteFont) {
                    auto& spriteIndex = font->spriteIndex;
                    sf::RenderTarget& currentRenderer = *(Game::get().getRenderTarget());

                    float cx = 0;
                    float cy = 0;

                    int strlength = string.length();
                    for (int i = 0; i < strlength; ++i) {
                        char c = string[i];
                        if (c == '\n') {
                            cx = 0;
                            cy += spriteIndex->height;
                            continue;
                        }
                        if (c != ' ') {
                            spriteIndex->draw(currentRenderer, { x + cx, y + cy }, font->charMap[string[i]], { 1, 1 }, color);
                        }
                        cx += spriteIndex->width + spacing;
                    }
                }
                else {
                    sf::Text t(font->fontIndex, string, size);
                    t.setFillColor(color);
                    t.setPosition({ x, y });
                    t.setLetterSpacing(spacing);
                    Game::get().getRenderTarget()->draw(t);
                }

                return 0;
            });
            lua_setfield(L, -2, "draw_font");
    lua_pop(L, 2);
}