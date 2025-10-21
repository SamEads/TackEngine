#pragma once

#ifndef M_PI
#define M_PI   3.14159265358979323846264338327950288
#endif

#include <SFML/Graphics.hpp>
#include <sol/sol.hpp>

inline float Rad2Deg(float radian) {
    return radian * (180.0f / M_PI);
}

inline float Deg2Rad(float radian) {
    return radian * (M_PI / 180.0f);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float PointDistance(float x1, float y1, float x2, float y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2) * 1.0);
}

inline sf::Color MakeColor(sol::table c) {
    uint8_t r = c.get<int>(1);
    uint8_t g = c.get<int>(2);
    uint8_t b = c.get<int>(3);
    uint8_t a = c.get<int>(4);
    return sf::Color{ r, g, b, a };
};