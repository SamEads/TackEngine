#pragma once

#include <SFML/Graphics.hpp>

struct CollisionResult {
    bool intersect;
    sf::Vector2f mtv; // min translation vector
};

CollisionResult polygonsIntersect(const std::vector<sf::Vector2f>& polyA, const std::vector<sf::Vector2f>& polyB);