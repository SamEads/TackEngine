#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include "luainc.h"
#include "util/profiler.h"
#include "util/timer.h"
#include "room/roomreference.h"
#include "object/objectid.h"
#include "gfx/sprite.h"

class Game {
public:
    unsigned int canvasWidth = 640;
    unsigned int canvasHeight = 480;
    Profiler profiler;
    bool letterbox = true;
    bool drawRoom = true;
    bool switchRooms = false;
    ObjectId nextRoomId;
    LuaState L;
    std::filesystem::path assetsFolder = "assets";
    std::unordered_map<std::string, RoomReference> roomReferences;
    Timer timer;
    float fps = 0;

    GFX::Canvas* currentRenderer;
    std::unique_ptr<sf::RenderWindow> window;
    float winX = 0, winY = 0;

    inline sf::RenderTarget* getRenderTarget() {
        if (currentRenderer == nullptr) {
            return window.get();
        }
        return &currentRenderer->rt;
    }
    inline float getCanvasPosX() {
        if (currentRenderer == nullptr) {
            return winX;
        }
        return currentRenderer->x;
    }
    inline float getCanvasPosY() {
        if (currentRenderer == nullptr) {
            return winY;
        }
        return currentRenderer->y;
    }
    sf::Shader* currentShader;

    void initializeLua(LuaState& L, const std::filesystem::path& assets);
    
    static Game& get() {
        static Game game;
        return game;
    }
};