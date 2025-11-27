#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include "luainc.h"
#include "util/profiler.h"
#include "util/timer.h"
#include "roomreference.h"
#include "objectid.h"

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

    sf::RenderTarget* currentRenderer;
    std::unique_ptr<sf::RenderTexture> consoleRenderer;
    std::unique_ptr<sf::RenderWindow> window;
    inline sf::RenderTarget* getRenderTarget() { return (currentRenderer == nullptr) ? window.get() : currentRenderer; }
    sf::Shader* currentShader;

    void initializeLua(LuaState& L, const std::filesystem::path& assets);
    
    static Game& get() {
        static Game game;
        return game;
    }
};