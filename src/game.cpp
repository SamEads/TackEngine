#include <iostream>
#include "game.h"
#include "drawable.h"

void Game::initializeLua(LuaState& L, const std::filesystem::path& assets) {
    // TODO:
    /*
    sol::table engineEnv = lua["TE"];

    lua.new_usertype<Game>(
        "Game",         sol::no_constructor,
        "fps",          sol::readonly(&Game::fps),
        sol::meta_function::index,      &Game::getKVP,
        sol::meta_function::new_index,  &Game::setKVP
    );
    
    engineEnv["game"] = this;

    engineEnv["game"]["set_size"] = [&](Game* self, unsigned int width, unsigned int height) {
        unsigned int lastCW = self->canvasWidth;
        unsigned int lastCH = self->canvasHeight;
        self->canvasWidth = std::max(1u, width);
        self->canvasHeight = std::max(1u, height);
    };

    engineEnv["game"]["set_tick_speed"] = [&](Game* self, double tickSpeed) {
        self->timer.setTickRate(tickSpeed);
    };

    auto windowModule = engineEnv.create_named("window");
    
    windowModule["set_caption"] = [&](const std::string& caption) {
        window->setTitle(caption);
    };

    windowModule["set_size"] = [&](unsigned int width, unsigned int height) {
        window->setSize({ width, height });
    };

    windowModule["get_width"] = [&]() {
        return Game::get().window->getSize().x;
    };

    windowModule["get_height"] = [&]() {
        return Game::get().window->getSize().y;
    };

    windowModule["center"] = [&]() {
        sf::Vector2u displaySize = sf::VideoMode::getDesktopMode().size;
        sf::Vector2u windowSize = Game::get().window->getSize();
        window->setPosition(sf::Vector2i {
            static_cast<int>((displaySize.x / 2) - (windowSize.x / 2)),
            static_cast<int>((displaySize.y / 2) - (windowSize.y / 2))
        });
    };

    auto gameRes = LuaScript(lua, std::filesystem::path(assets / "scripts" / "game.lua"));
    if (!gameRes.valid()) {
        sol::error e = gameRes;
        std::cout << e.what() << "\n";
    }
    */
}