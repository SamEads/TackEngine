#include <fstream>
#include "room.h"
#include "game.h"
#include "tileset.h"
#include "vendor/json.hpp"

using namespace nlohmann;

Room::Room(sol::state& lua, const std::string room) : lua(lua) {
    auto& game = Game::get();
    std::filesystem::path path = game.assetsFolder / "rooms" / room;
    std::filesystem::path jsonPath = path / "data.json";

    std::ifstream i(jsonPath);
    json j = json::parse(i);

    auto& objMgr = ObjectManager::get();

    width = j["room_settings"]["width"];
    height = j["room_settings"]["height"];

    cameraWidth = 256;
    cameraHeight = 224;

    for (auto& l : j["layers"]) {
        std::string type = l["layer_type"].get<std::string>();
        int depth = l["depth"];
        if (type == "instances") {
            for (auto& i : l["instances"]) {
                std::string objectIndex = i["object_index"];
                float x = i["x"];
                float y = i["y"];
                auto obj = instanceCreate(x, y, depth, objMgr.baseClasses[objectIndex].object.as<std::unique_ptr<Object>&>());
                obj->xScale = i["scale_x"];
                obj->yScale = i["scale_y"];
                obj->imageAngle = i["rotation"];
                obj->imageIndex = i["image_index"];
                obj->imageSpeedMod = i["image_speed"];

                obj->xPrev = obj->x;
                obj->yPrev = obj->y;
            }
        }
        if (type == "background") {
            std::unique_ptr<Background> bg = std::make_unique<Background>();
            bg->tiledX = l["tiled_x"];
            bg->tiledY = l["tiled_y"];
            bg->speedX = l["speed_x"];
            bg->speedY = l["speed_y"];
            bg->x = l["x"];
            bg->y = l["y"];
            bg->visible = l["visible"];
            bg->depth = depth;
            bg->name = l["name"];
            if (!l["sprite"].is_null()) {
                bg->spriteIndex = &SpriteManager::get().sprites[l["sprite"]];
            }
            auto& col = l["color"];
            if (col.size() == 4) {
                bg->color = { col[0].get<uint8_t>(), col[1].get<uint8_t>(), col[2].get<uint8_t>(), col[3].get<uint8_t>() };
            }
            backgrounds.push_back(std::move(bg));
        }
        if (type == "tiles") {
            std::unique_ptr<Tilemap> map = std::make_unique<Tilemap>();
            map->depth = depth;
            map->tileCountX = l["width"];
            map->tileCountY = l["height"];
            map->visible = l["visible"];
            map->name = l["name"];
            map->tileData = l["tiles"].get<std::vector<unsigned int>>();
            map->tileset = &TilesetManager::get().tilesets[l["tileset"].get<std::string>()];
            tilemaps.push_back(std::move(map));
        }
    }

    addQueue();
    for (auto& objUnique : instances) {
        auto it = objUnique->kvp.find("create");
        if (it != objUnique->kvp.end()) {
            Object* obj = objUnique.get();
            auto res = it->second.as<sol::safe_function>()(obj, this);
            if (!res.valid()) {
                sol::error e = res;
                std::cout << e.what() << "\n";
            }
        }
    }
}

void Room::update() {
    for (auto& i : instances) {
        i->xPrev = i->x;
        i->yPrev = i->y;
        i->imageIndex += (i->imageSpeed * i->imageSpeedMod);
    }

    for (auto& i : instances) {
        i->beginStep(this);
    }
    addQueue();
    for (auto& i : instances) {
        i->step(this);
    }
    addQueue();
    for (auto& i : instances) {
        i->endStep(this);
    }
    addQueue();
}

void Room::draw() {
    auto target = Game::get().currentRenderer;

    sf::View view = target->getView();
    float sw = cameraWidth;
    float sh = cameraHeight;
    view.setSize({ sw, sh });
    view.setCenter({ floorf(cameraX) + floorf(sw / 2.0f), floorf(cameraY) + floorf(sh / 2.0f) });
    target->setView(view);
    
    std::vector<Drawable*> drawables;
    int count = 0;
    for (auto& bg : backgrounds) {
        if (bg->visible) {
            count++;
        }
    }
    for (auto& tm : tilemaps) {
        if (tm->visible) {
            count++;
        }
    }
    for (auto& i : instances) {
        if (i->visible) {
            count++;
        }
    }
    drawables.reserve(count);

    for (auto& bg : backgrounds) {
        if (bg->visible) {
            drawables.push_back(bg.get());
            bg->draw(this);
        }
    }
    for (auto& tm : tilemaps) {
        if (tm->visible) {
            drawables.push_back(tm.get());
            tm->draw(this);
        }
    }
    for (auto& i : instances) {
        if (i->visible) {
            drawables.push_back(i.get());
            i->draw(this);
        }
    }

    std::sort(drawables.begin(), drawables.end(), [](const Drawable* a, const Drawable* b) {
        return a->depth > b->depth;
    });

    for (auto& d : drawables) {
        d->draw(this);
    }

    target->setView(target->getDefaultView());
}

void Room::setCameraX(float val) {
    cameraX = val;
    if (cameraX < 0) {
        cameraX = 0;
    }
    if (cameraX > width - cameraWidth) {
        cameraX = width - cameraWidth;
    }
}

void Room::setCameraY(float val) {
    cameraY = val;
    if (cameraY < 0) {
        cameraY = 0;
    }
    if (cameraY > height - cameraHeight) {
        cameraY = height - cameraHeight;
    }
}

void Tilemap::draw(Room* room) {
    int tileWidth = tileset->tileWidth;
    int tileHeight = tileset->tileHeight;

    int thisCx = (room->cameraX / tileWidth) - 1;
    int thisW = static_cast<int>(ceilf(room->cameraWidth / (float)tileWidth)) + 2;
    
    int thisCy = (room->cameraY / tileHeight) - 1;
    int thisH = static_cast<int>(ceilf(room->cameraHeight / (float)tileHeight)) + 2;

    int fullW = thisCx + std::min(thisW, tileCountX + 1);
    int fullH = thisCy + std::min(thisH, tileCountY + 1);

    SpriteIndex* spriteIndex = tileset->spriteIndex;
    sf::Sprite* s = spriteIndex->sprite.get();
    s->setScale({ 1, 1 });
    s->setOrigin({ 0, 0 });
    s->setRotation(sf::degrees(0));
    s->setColor({ 255, 255, 255, 255 });
    auto target = Game::get().currentRenderer;

    int totalTiles = tileData.size();
    for (int xx = thisCx; xx < fullW; ++xx) {
        for (int yy = thisCy; yy < fullH; ++yy) {
            int pos = xx + (yy * tileCountX);
            if (pos >= totalTiles || pos < 0) {
                continue;
            }

            int tile = tileData[pos];
            if (tile == 0) {
                continue;
            }

            int tileX = tile % tileset->tileCountX;
            int tileY = tile / tileset->tileCountX;

            s->setTextureRect(sf::IntRect(
                { tileX * (int)tileWidth + (tileX * tileset->separationX), tileY * (int)tileHeight + (tileY * tileset->separationY) },
                { (int)tileWidth, (int)tileHeight }
            ));
            s->setPosition({ floorf(xx * (float)tileWidth), floorf(yy * (float)tileHeight) });
            target->draw(*s);
        }
    }
}

void Background::draw(Room* room) {
    float x = room->cameraX - 10;
    float y = room->cameraY - 10;

    if (spriteIndex) {
        sf::Sprite* spr = spriteIndex->sprite.get();
        spr->setScale({ 1, 1 });
        spr->setOrigin({ 0, 0 });
        spr->setColor(color);
        spr->setRotation(sf::degrees(0));
        spr->setColor({ 255, 255, 255, 255 });
        spr->setColor(sf::Color::White);
        float parallax = 0.0f;
        float parallaxY = 0.0f;
        float x = (room->cameraX * parallax) + this->x;
        float timesOver = floorf((room->cameraX * (1.0f - parallax)) / spriteIndex->width);
        x += (spriteIndex->width) * timesOver;

        float y = (room->cameraY * parallaxY) + this->y;
        timesOver = floorf((room->cameraY * (1.0f - parallaxY)) / spriteIndex->height);
        y += (spriteIndex->height) * timesOver;

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                spr->setPosition({ floorf(x) + (i * spriteIndex->width), floorf(y) + (j * spriteIndex->height) });
                Game::get().currentRenderer->draw(*spr);
            }
        }
    }
    else {
        sf::RectangleShape rs({ room->cameraWidth + 20, room->cameraHeight + 20 });
        rs.setFillColor(color);
        rs.setPosition({ x, y });
        Game::get().currentRenderer->draw(rs);
    }
}