#include "game.h"

Room* Game::queueRoom(RoomReference* room) {
    switchRooms = true;
    if (room == nullptr) {
        queuedRoom = std::make_unique<Room>(lua);
    }
    else {
        queuedRoom = std::make_unique<Room>(lua, *room);
    }
    queuedRoom->myId = roomId++;
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
        "queue_room",     sol::readonly(&Game::queueRoom),
        "room",         sol::readonly_property(&Game::getRoom),
        "get_width", [&](Game* game) {
            return window->getSize().x;
        },
        "get_height", [&](Game* game) {
            return window->getSize().y;
        },
        "draw_room", &Game::drawRoom,
        sol::meta_function::index,      &Game::getKVP,
        sol::meta_function::new_index,  &Game::setKVP
    );
    
    lua["game"] = this;

    lua["game"]["set_size"] = [&](Game* self, unsigned int width, unsigned int height) {
        unsigned int lastCW = self->canvasWidth;
        unsigned int lastCH = self->canvasHeight;
        self->canvasWidth = std::max(1u, width);
        self->canvasHeight = std::max(1u, height);
        if (self->canvasWidth != lastCW || self->canvasHeight != lastCH) {
            // bool resizedCanvas = self->consoleRenderer->resize(sf::Vector2u { self->canvasWidth, self->canvasHeight });
        }

        if (self->room) {
            room->camera.width = self->canvasWidth;
            room->camera.height = self->canvasHeight;
        }
        if (self->queuedRoom && self->queuedRoom != self->room) {
            queuedRoom->camera.width = self->canvasWidth;
            queuedRoom->camera.height = self->canvasHeight;
        }
    };

    lua["game"]["set_tick_speed"] = [&](Game* self, double tickSpeed) {
        self->timer.setTickRate(tickSpeed);
    };

    lua["window"] = lua.create_table();
    
    lua["window"]["set_caption"] = [&](const std::string& caption) {
        window->setTitle(caption);
    };

    lua["window"]["set_size"] = [&](unsigned int width, unsigned int height) {
        window->setSize({ width, height });
    };

    lua["window"]["center"] = [&]() {
        sf::Vector2u displaySize = sf::VideoMode::getDesktopMode().size;
        sf::Vector2u windowSize = Game::get().window->getSize();
        window->setPosition(sf::Vector2i { static_cast<int>((displaySize.x / 2) - (windowSize.x / 2)), static_cast<int>((displaySize.y / 2) - (windowSize.y / 2)) });
    };

    auto gameRes = LuaScript(lua, std::filesystem::path(assets / "scripts" / "game.lua"));
    if (!gameRes.valid()) {
        sol::error e = gameRes;
        std::cout << e.what() << "\n";
    }
}