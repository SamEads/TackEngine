#pragma once

#include <filesystem>

class RoomReference {
public:
    std::string name;
    std::filesystem::path p;
};