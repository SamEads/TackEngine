#pragma once

#include <filesystem>

class Room;

class Drawable {
public:
    int depth = 0;
    bool visible = true;
    bool drawsGui = false;
    virtual void draw(Room* room, float alpha) {}
    virtual void beginDraw(Room* room, float alpha) {}
    virtual void endDraw(Room* room, float alpha) {}
    virtual void drawGui(Room* room, float alpha) {}
};