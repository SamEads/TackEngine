#pragma once

#include <SFML/Graphics.hpp>
#include <sol/sol.hpp>

class Keys {
public:
	std::array<bool, sf::Keyboard::ScancodeCount> keys;
	std::array<bool, sf::Keyboard::ScancodeCount> keysLast;
	void update();
	bool pressed(sf::Keyboard::Scancode key);
	bool held(sf::Keyboard::Scancode key);
	bool released(sf::Keyboard::Scancode key);
	static Keys& get() {
		static Keys keys;
		return keys;
	}
	static void InitializeLuaEnums(sol::state& lua);
};