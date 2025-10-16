#include <iostream>
#include "object.h"
#include "game.h"
#include "room.h"

void Object::beginStep(Room* room) {
	runScript("begin_step", room);
}

void Object::step(Room* room) {
	runScript("step", room);
}

void Object::endStep(Room* room) {
	runScript("end_step", room);
}

void Object::draw(Room* room) {
	if (runScript("draw", room) || !spriteIndex) {
		return;
	}
	spriteIndex->draw(*Game::get().currentRenderer, { floorf(x), floorf(y) }, imageIndex, { xScale, yScale }, sf::Color::White, imageAngle);
}
