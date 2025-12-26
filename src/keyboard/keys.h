#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <array>
#include "luainc.h"

class Keys {
public:
	std::array<bool, sf::Keyboard::ScancodeCount> keys;
	std::array<bool, sf::Keyboard::ScancodeCount> keysLast;
	void update(bool windowFocused);
	bool pressed(sf::Keyboard::Scancode key);
	bool held(sf::Keyboard::Scancode key);
	bool released(sf::Keyboard::Scancode key);
	static Keys& get() {
		static Keys keys;
		return keys;
	}
	void initializeLua(LuaState L);
};