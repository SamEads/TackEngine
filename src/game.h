#pragma once

#include <SFML/Graphics.hpp>

class Game {
public:
    sf::RenderTarget* currentRenderer;
    std::unique_ptr<sf::RenderWindow> window;
    std::filesystem::path assetsFolder = "assets";
    static Game& get() {
        static Game game;
        return game;
    }
};