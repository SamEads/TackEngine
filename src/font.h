#pragma once

#include <SFML/Graphics.hpp>
#include <sol/sol.hpp>
#include "sprite.h"

class Font {
public:
    SpriteIndex* spriteIndex;
    std::unordered_map<char, int> charMap;
};

class FontManager {
public:
    std::unordered_map<std::string, Font> fonts;
    void initializeLua(sol::state& lua);
    static FontManager& get() {
        static FontManager fm;
        return fm;
    }
};