#include "game.h"

Room* Game::queueRoom(const RoomReference& room) {
    queuedRoom = std::make_unique<Room>(lua, room);
    queuedRoom->load();
    return queuedRoom.get();
}

Room *Game::getRoom() {
    return this->room.get();
}

void Game::initializeLua(sol::state &state, const std::filesystem::path& assets) {
    lua.new_usertype<Game>(
        "Game",         sol::no_constructor,
        "fps",          sol::readonly(&Game::fps),
        "queue_room",   sol::readonly(&Game::queueRoom),
        "room",         sol::readonly_property(&Game::getRoom),
        sol::meta_function::index,      &Game::getKVP,
        sol::meta_function::new_index,  &Game::setKVP
    );
    
    lua["game"] = this;

    lua["game"]["set_tick_speed"] = [&](Game* self, double tickSpeed) {
        self->timer.setTickRate(tickSpeed);
    };
    
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

    lua["game"]["set_letterboxing"] = [](Game* self, bool letterbox) {
        self->letterbox = letterbox;
    };

    auto gameRes = lua.safe_script_file(std::filesystem::path(assets / "scripts" / "game.lua").string());
    if (!gameRes.valid()) {
        sol::error e = gameRes;
        std::cout << e.what() << "\n";
    }
}