#include <iostream>
#include "object.h"
#include "game.h"
#include "room.h"

bool Object::runRoomScript(const std::string& script, Room* room) {
	auto step = kvp.find(script);
	if (step != kvp.end()) {
		auto res = step->second.as<sol::safe_function>()(this, room);
		if (!res.valid()) {
			sol::error e = res;
			std::cout << e.what() << "\n";
		}
		else {
			return true;
		}
	}
	return false;
}

void Object::beginStep(Room* room) {
	runRoomScript("begin_step", room);
}

void Object::step(Room* room) {
	runRoomScript("step", room);
}

void Object::endStep(Room* room) {
	runRoomScript("end_step", room);
}

void Object::draw(Room* room) {
	if (runRoomScript("draw", room) || !spriteIndex) {
		return;
	}
	spriteIndex->draw(*Game::get().currentRenderer, { floorf(x), floorf(y) }, imageIndex, { xScale, yScale }, sf::Color::White, imageAngle);
}
