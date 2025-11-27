#pragma once

#include "luainc.h"
#include "sprite.h"

class Font {
public:
    bool isSpriteFont;
    SpriteIndex* spriteIndex;
    sf::Font fontIndex;
    std::unordered_map<char, int> charMap;
};

class FontManager {
public:
    std::unordered_map<std::string, Font> fonts;
    void initializeLua(LuaState& L, std::filesystem::path assets);
    static FontManager& get() {
        static FontManager fm;
        return fm;
    }
};