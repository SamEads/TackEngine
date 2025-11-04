#pragma once

#ifdef USE_RAYLIB_BACKEND
#include <raylib.h>
using Vector2f = Vector2;
using c_Color = Color;
#else
#include <SFML/Graphics.hpp>
using Vector2f = sf::Vector2f;
using c_Color = sf::Color;
#endif