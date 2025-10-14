#pragma once

#ifndef M_PI
#define M_PI   3.14159265358979323846264338327950288
#endif

inline float Rad2Deg(float radian) {
    return radian * (180.0f / M_PI);
}

inline float Deg2Rad(float radian) {
    return radian * (M_PI / 180.0f);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}