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

void Object::draw(Room* room, float alpha) {
	if (runScript("draw", room, alpha) || !spriteIndex) {
		return;
	}

	float interpX = lerp(xPrev, x, alpha);
	float interpY = lerp(yPrev, y, alpha);
	spriteIndex->draw(*Game::get().currentRenderer, { floorf(interpX), floorf(interpY) }, imageIndex, { xScale, yScale }, sf::Color::White, imageAngle);
}

void Object::drawGui(Room *room, float alpha) {
	runScript("draw_gui", room, alpha);
}
