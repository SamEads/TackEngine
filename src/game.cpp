#include "game.h"

void Game::gotoRoom(const RoomReference& room) {
    this->room = std::make_unique<Room>(lua, room);
}