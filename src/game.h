#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <sol/sol.hpp>
#include "util/profiler.h"
#include "util/timer.h"
#include "roomreference.h"
#include "objectid.h"

class Game {
public:
    unsigned int canvasWidth = 640;
    unsigned int canvasHeight = 480;
    Profiler profiler;
    sol::table engineEnv;
    sol::state lua;
    bool letterbox = true;
    bool drawRoom = true;
    bool switchRooms = false;
    ObjectId nextRoomId;
    std::filesystem::path assetsFolder = "assets";
    std::unordered_map<std::string, RoomReference> roomReferences;
    std::unordered_map<std::string, sol::object> kvp;
    Timer timer;
    float fps = 0;

    sf::RenderTarget* currentRenderer;
    std::unique_ptr<sf::RenderTexture> consoleRenderer;
    std::unique_ptr<sf::RenderWindow> window;
    inline sf::RenderTarget* getRenderTarget() { return (currentRenderer == nullptr) ? window.get() : currentRenderer; }
    sf::Shader* currentShader;

    void initializeLua(sol::state& state, const std::filesystem::path& assets);
    
    static Game& get() {
        static Game game;
        return game;
    }

    void setKVP(const std::string& key, sol::main_object obj) {
        auto it = kvp.find(key);
        if (it == kvp.end()) {
            kvp.insert({ key, sol::object(std::move(obj)) });
        }
        else {
            it->second = sol::object(std::move(obj));
        }
    }

    sol::object getKVP(const std::string& ref) {
        auto it = kvp.find(ref);
        if (it == kvp.end()) {
            return sol::lua_nil;
        }
        return it->second;
    }
};