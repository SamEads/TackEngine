#pragma once

#include <SFML/Graphics.hpp>
#include <sol/sol.hpp>
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
    void initializeLua(sol::state& lua, std::filesystem::path assets);
    static FontManager& get() {
        static FontManager fm;
        return fm;
    }
};