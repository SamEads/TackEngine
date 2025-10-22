#include "game.h"

void Game::gotoRoom(const RoomReference& room) {
    this->queuedRoom = std::make_unique<Room>(lua, room);
}

void Game::initializeLua(sol::state &state, const std::filesystem::path& assets) {
    lua.new_usertype<Game>(
        "Game",         sol::no_constructor,
        "fps",          sol::readonly(&Game::fps),
        "room_goto",    sol::readonly(&Game::gotoRoom),
        sol::meta_function::index,      &Game::getKVP,
        sol::meta_function::new_index,  &Game::setKVP
    );
    
    lua["game"] = this;
    lua["game"]["set_caption"] = [&](Game* self, const std::string& caption) {
        self->window->setTitle(caption);
    };

    lua["game"]["set_size"] = [&](Game* self, unsigned int width, unsigned int height) {
        self->window->setSize({ width, height });
    };

    lua["game"]["center_window"] = [&](Game* self) {
        sf::Vector2u displaySize = sf::VideoMode::getDesktopMode().size;
        sf::Vector2u windowSize = Game::get().window->getSize();
        self->window->setPosition(sf::Vector2i { static_cast<int>((displaySize.x / 2) - (windowSize.x / 2)), static_cast<int>((displaySize.y / 2) - (windowSize.y / 2)) });
    };

    auto gameRes = lua.safe_script_file(std::filesystem::path(assets / "scripts" / "game.lua").string());
    if (!gameRes.valid()) {
        sol::error e = gameRes;
        std::cout << e.what() << "\n";
    }
}