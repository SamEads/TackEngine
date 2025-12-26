#include "collision.h"
#include <cmath>

CollisionResult polygonsIntersect(const std::vector<sf::Vector2f>& polyA, const std::vector<sf::Vector2f>& polyB) {
    if (polyA.empty() || polyB.empty()) return { false, {} }; // no collision

    float smallestOverlap = std::numeric_limits<float>::max();
    sf::Vector2f smallestAxis;

    auto getAxes = [](const std::vector<sf::Vector2f>& poly) {
        std::vector<sf::Vector2f> axes;

        for (size_t i = 0; i < poly.size(); ++i) {
            sf::Vector2f p1 = poly[i];
            sf::Vector2f p2 = poly[(i + 1) % poly.size()];
            sf::Vector2f edge = p2 - p1;
            sf::Vector2f normal(-edge.y, edge.x);

            float length = std::sqrt(normal.x * normal.x + normal.y * normal.y);
            if (length > 0) normal /= length;

            axes.push_back(normal);
        }

        return axes;
    };

    auto project = [](const std::vector<sf::Vector2f>& poly, const sf::Vector2f& axis) {
        float min = poly[0].x * axis.x + poly[0].y * axis.y;
        float max = min;

        for (const auto& p : poly) {
            float proj = p.x * axis.x + p.y * axis.y;

            if (proj < min) min = proj;
            if (proj > max) max = proj;
        }

        return std::make_pair(min, max);
    };

    auto axes = getAxes(polyA);
    auto axesB = getAxes(polyB);
    axes.insert(axes.end(), axesB.begin(), axesB.end());

    for (const auto& axis : axes) {
        auto [minA, maxA] = project(polyA, axis);
        auto [minB, maxB] = project(polyB, axis);

        float overlap = std::min(maxA, maxB) - std::max(minA, minB);
        if (overlap <= 0) return { false, {} }; // no collision

        if (overlap < smallestOverlap) {
            smallestOverlap = overlap;
            smallestAxis = axis;
        }
    }

    return { true, smallestAxis * smallestOverlap };
}