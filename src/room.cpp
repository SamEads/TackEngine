#include <fstream>
#include "room.h"
#include "game.h"
#include "tileset.h"
#include "vendor/json.hpp"

using namespace nlohmann;

void Room::initializeLua(sol::state &lua, const std::filesystem::path &assets) {
    lua.new_usertype<Tilemap>(
        "Tilemap", sol::no_constructor,
        "visible", &Tilemap::visible,
        "depth", &Tilemap::depth
    );

    lua.new_usertype<Background>(
        "Background", sol::no_constructor,
        "visible", &Background::visible,
        "depth", &Background::depth,
        "sprite_index", &Background::spriteIndex
    );

    lua.new_usertype<Room>(
        "Room", sol::no_constructor,

        // Room info
        "width", sol::readonly(&Room::width),
        "height", sol::readonly(&Room::height),
        "camera_x", sol::property(&Room::getCameraX, &Room::setCameraX),
        "camera_xprevious", sol::readonly(&Room::cameraPrevX),
        "camera_yprevious", sol::readonly(&Room::cameraPrevY),
        "camera_y", sol::property(&Room::getCameraY, &Room::setCameraY),
        "camera_width", sol::readonly(&Room::cameraWidth),
        "camera_height", sol::readonly(&Room::cameraHeight),

        // Objects & instances
        "instance_create", &Room::instanceCreateScript,
        "instance_exists", &Room::instanceExistsScript,
        "instance_destroy", &Room::instanceDestroyScript,
        "object_count", &Room::objectCount,
        "object_get", &Room::getObject,
        "object_exists", &Room::objectExists,
        "object_get_list", &Room::objectGetList,
        "object_destroy", &Room::objectDestroy,

        // Layers
        "tile_layer_get", &Room::getTileLayer,
        "background_layer_get", &Room::getBackgroundLayer,

        // Collisions
        "instance_place", &Room::instancePlaceScript,
        "collision_rectangle", &Room::collisionRectangleScript,
        "collision_rectangle_list", &Room::collisionRectangleList,

        "render_x", &Room::renderCameraX,
        "render_y", &Room::renderCameraY,
        "set_render_position", &Room::setView,

        sol::meta_function::index,      &Room::getKVP,
        sol::meta_function::new_index,  &Room::setKVP
    );

    for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "rooms")) {
        if (!it.is_directory()) {
            continue;
        }

        std::filesystem::path p = it.path();
        std::string identifier = p.filename().string();

        RoomReference& ref = Game::get().rooms[identifier];
        ref.name = identifier;
        ref.p = p;

        lua[identifier] = ref;
    }
}

Room::Room(sol::state &lua, const RoomReference &room) : lua(lua) {
    roomReference = &room;
}

void Room::load() {
    auto jsonPath = roomReference->p / "data.json";
    std::ifstream i(jsonPath);
    json j = json::parse(i);

    auto& objMgr = ObjectManager::get();

    width = j["room_settings"]["width"];
    height = j["room_settings"]["height"];

    cameraWidth = 256;
    cameraHeight = 224;

    for (auto& l : j["layers"]) {
        std::string type = l["type"].get<std::string>();
        int depth = l["depth"];

        if (type == "objects") {
            for (auto& i : l["objects"]) {
                std::string objectIndex = i["object"];
                float x = i["x"];
                float y = i["y"];
                BaseObject* base = objMgr.baseClasses[objectIndex].object.as<BaseObject*>();
                auto obj = instanceCreate(x, y, depth, objMgr.baseClasses[objectIndex].object.as<BaseObject*>());
                obj->xScale = i.value("scale_x", 1.0f);
                obj->yScale = i.value("scale_y", 1.0f);
                obj->imageAngle = i.value("angle", 0.0f);
                obj->imageIndex = i.value("image_index", 0.0f);
                obj->imageSpeedMod = i.value("image_speed", 1.0f);
                if (i["properties"].size() > 0) {
                    bool hadProperties = false;
                    if (obj->kvp.find("properties") == obj->kvp.end()) {
                        obj->setDyn("properties", sol::table(lua, sol::create));
                    }
                    sol::table props = obj->getDyn("properties");
                    auto& origProps = obj->self->rawProperties;
                    for (auto& prop : i["properties"]) {
                        for (auto& [k, v] : prop.items()) {
                            auto& origProp = origProps.find(k);
                            if (origProp == origProps.end()) {
                                continue;
                            }
                            props[k] = FieldCreateFromProperty(k, origProp->second.first, v, lua);
                        }
                    }
                }

                obj->xPrev = obj->x;
                obj->yPrev = obj->y;
            }
        }

        if (type == "background") {
            std::unique_ptr<Background> bg = std::make_unique<Background>();
            bg->tiledX = l["tiled_x"], bg->tiledY = l["tiled_y"];
            bg->speedX = l["speed_x"], bg->speedY = l["speed_y"];
            bg->x = l["x"], bg->y = l["y"];
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
            if (!l["compressed"]) {
                map->tileData = l["tiles"].get<std::vector<unsigned int>>();
            }
            else {
                auto compressed = l["tiles"].get<std::vector<int>>();
                auto& decompressed = map->tileData;
		        decompressed.reserve(map->tileCountX * map->tileCountY);
                int size = compressed.size();
                for (int i = 0; i < size;) {
                    int value = compressed[i++];

                    // start a value train
                    if (value >= 0) {
                        while (true) {
                            // stay in bounds
                            if (i >= size) {
                                break;
                            }

                            int nextValue = compressed[i++];

                            if (nextValue >= 0) {
                                decompressed.push_back(nextValue);
                            }
                            else {
                                value = nextValue;
                                break;
                            }
                        }
                    }

                    // Negative value is count
                    if (value < 0) {
                        // stay in bounds
                        if (i >= size) {
                            break;
                        }

                        int repeatValue = compressed[i++];

                        for (int i = 0; i < -value; ++i) {
                            decompressed.push_back(repeatValue);
                        }
                    }
                }
            }
            int pos = 0;
            map->tileset = &TilesetManager::get().tilesets[l["tileset"].get<std::string>()];
            tilemaps.push_back(std::move(map));
        }
    }

    createAndRoomStartEvents();
}

// Room -> "Create"
// Instances -> "Create"
// Room -> Creation Code
// Room -> "Room Start"
// Instances -> "Room Start"
void Room::createAndRoomStartEvents() {
    auto& game = Game::get();

    auto create = kvp.find("create");
    if (create != kvp.end()) {
        create->second.as<sol::safe_function>()(this);
    }
    addQueue();

    for (auto& objUnique : instances) {
        objUnique->runScript("create", this);
    }
    addQueue();

    // Room creation code specific to this room
    lua["room"] = this;
    std::filesystem::path roomScript = game.assetsFolder / "scripts" / "rooms" / std::string(roomReference->name + ".lua");
    if (std::filesystem::exists(roomScript)) {
        auto result = lua.safe_script_file(roomScript.string());
        if (!result.valid()) {
            sol::error e = result;
            std::cout << e.what() << "\n";
        }
    }
    lua["room"] = nullptr;
    addQueue();

    auto start = kvp.find("room_start");
    if (start != kvp.end()) {
        start->second.as<sol::safe_function>()(this);
        addQueue();
    }

    addQueue();
    for (auto& objUnique : instances) {
        objUnique->runScript("room_start", this);
    }

    cameraPrevX = cameraX;
    cameraPrevY = cameraY;
}

void Room::update() {
    cameraPrevX = cameraX;
    cameraPrevY = cameraY;

    for (auto& i : instances) {
        i->xPrev = i->x;
        i->yPrev = i->y;
        if (i->incrementImageSpeed) {
            i->imageIndex += (i->imageSpeed * i->imageSpeedMod);
        }
    }

    // Begin Step
    Game::get().profiler.start("beginstep");
    for (auto& instance : instances) {
        instance->runScript("begin_step", this);
    }
    addQueue();
    Game::get().profiler.finish("beginstep");

    // Step
    Game::get().profiler.start("step");
    for (auto& instance : instances) {
        instance->runScript("step", this);
    }
    addQueue();
    Game::get().profiler.finish("step");

    // End Step
    Game::get().profiler.start("endstep");
    for (auto& instance : instances) {
        instance->runScript("end_step", this);
    }
    addQueue();
    Game::get().profiler.finish("endstep");
}

void Room::setView(float cx, float cy) {
    auto target = Game::get().currentRenderer;

    sf::Vector2u gameSize = target->getSize();
    sf::View view({ { 0, 0 }, { cameraWidth, cameraHeight } });

    view.setCenter({ cx + cameraWidth / 2.0f, cy + cameraHeight / 2.0f });
    target->setView(view);

    renderCameraX = cx;
    renderCameraY = cy;
}


void Room::draw(float alpha) {
    auto target = Game::get().currentRenderer;
    setView(lerp(cameraPrevX, cameraX, alpha), lerp(cameraPrevY, cameraY, alpha));
    
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
        }
    }
    for (auto& tm : tilemaps) {
        if (tm->visible) {
            drawables.push_back(tm.get());
        }
    }
    for (auto& i : instances) {
        if (i->visible) {
            drawables.push_back(i.get());
        }
    }

    std::sort(drawables.begin(), drawables.end(), [](const Drawable* a, const Drawable* b) {
        return a->depth > b->depth;
    });

    for (auto& d : drawables) {
        d->beginDraw(this, alpha);
    }

    for (auto& d : drawables) {
        d->draw(this, alpha);
    }

    for (auto& d : drawables) {
        d->endDraw(this, alpha);
    }

    /*
    sf::RectangleShape rs;
    rs.setPosition({ renderCameraX, renderCameraY });
    rs.setSize({ cameraWidth, cameraHeight });
    rs.setOutlineColor({ 255, 0, 0, 255 });
    rs.setOutlineThickness(1);
    rs.setFillColor({ 0, 0, 0, 0 });
    target->draw(rs);
    */

    target->setView(target->getDefaultView());

    for (auto& d : drawables) {
        if (!d->drawsGui) continue;
        d->drawGui(this, alpha);
    }
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

std::vector<Object::Reference> Room::objectGetList(BaseObject* baseType) {
    std::vector<Object::Reference> v;
    for (auto& i : instances) {
        if (i->extends(baseType)) {
            v.push_back(i->MyReference);
        }
    }
    return v;
}

void Background::draw(Room* room, float alpha) {
    float cx = room->renderCameraX;
    float cy = room->renderCameraY;

    float x = cx - 1;
    float y = cy - 1;

    if (spriteIndex) {
        sf::Sprite* spr = spriteIndex->sprite.get();
        spr->setScale({ 1, 1 });
        spr->setOrigin({ 0, 0 });
        spr->setColor(color);
        spr->setRotation(sf::degrees(0));
        spr->setColor(sf::Color::White);
        float parallax = speedX;
        float parallaxY = speedY;
        float x = (cx * parallax) + this->x;
        float timesOver = floorf((cx * (1.0f - parallax)) / spriteIndex->width);
        x += (spriteIndex->width) * timesOver;

        float y = (cy * parallaxY) + this->y;
        timesOver = floorf((cy * (1.0f - parallaxY)) / spriteIndex->height);
        y += (spriteIndex->height) * timesOver;

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (!tiledY && j != 0) continue;
                spr->setPosition({ floorf(x) + (i * spriteIndex->width), floorf(y) + (j * spriteIndex->height) });
                Game::get().currentRenderer->draw(*spr);
            }
        }
    }
    else {
        sf::RectangleShape rs({ room->cameraWidth + 2, room->cameraHeight + 2 });
        rs.setTexture(&SpriteManager::get().whiteTexture);
        rs.setFillColor(color);
        rs.setPosition({ x, y });
        Game::get().currentRenderer->draw(rs);
    }
}