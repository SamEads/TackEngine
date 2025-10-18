#pragma once

#include <SFML/Graphics.hpp>
#include "sprite.h"

class Font {
public:
    SpriteIndex* spriteIndex;
    std::unordered_map<char, int> charMap;
};

class FontManager {
public:
    std::unordered_map<std::string, Font> fonts;
    static FontManager& get() {
        static FontManager fm;
        return fm;
    }
};