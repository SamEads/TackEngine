#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI   3.14159265358979323846264338327950288
#endif

#include <SFML/Graphics.hpp>

inline float Rad2Deg(float radian) {
    return radian * (180.0f / M_PI);
}

inline float Deg2Rad(float radian) {
    return radian * (M_PI / 180.0f);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float signum(float number) {
    if (number > 0) return 1;
    if (number < 0) return -1;
    return 0;
}

inline float PointDistance(float x1, float y1, float x2, float y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2) * 1.0);
}

inline sf::Color MakeColor(std::tuple<int, int, int, int> c) {
    uint8_t r = std::get<0>(c);
    uint8_t g = std::get<1>(c);
    uint8_t b = std::get<2>(c);
    uint8_t a = std::get<3>(c);
    return { r, g, b, a };
};
