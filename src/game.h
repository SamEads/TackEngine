#pragma once

#include <SFML/Graphics.hpp>
#include "room.h"

class Game {
public:
    sol::state lua;
    float fps = 0;
    sf::RenderTarget* currentRenderer;
    std::unique_ptr<sf::RenderTexture> consoleRenderer;
    std::unique_ptr<sf::RenderWindow> window;
    std::unique_ptr<Room> room;
    std::filesystem::path assetsFolder = "assets";
    std::unordered_map<std::string, RoomReference> rooms;
    static Game& get() {
        static Game game;
        return game;
    }
    void gotoRoom(const RoomReference& room);
    std::unordered_map<std::string, sol::object> kvp;
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