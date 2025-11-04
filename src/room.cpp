#include <fstream>
#include "room.h"
#include "game.h"
#include "tileset.h"
#include "vendor/json.hpp"

using namespace nlohmann;

sol::function drawAll;
void Room::initializeLua(sol::state &lua, const std::filesystem::path &assets) {
    Tilemap::initializeLua(lua);

    lua.new_usertype<Background>(
        "Background", sol::no_constructor,
        "visible", &Background::visible,
        "depth", &Background::depth,
        "sprite_index", &Background::spriteIndex
    );

    lua.new_usertype<Room>(
        "Room", sol::no_constructor,

        // Room info
        "width",    sol::readonly(&Room::width),
        "height",   sol::readonly(&Room::height),
        "camera_x", sol::property(&Room::getCameraX, &Room::setCameraX),
        "camera_xprevious", sol::readonly(&Room::cameraPrevX),
        "camera_yprevious", sol::readonly(&Room::cameraPrevY),
        "camera_y",     sol::property(&Room::getCameraY, &Room::setCameraY),
        "camera_width",     sol::readonly(&Room::cameraWidth),
        "camera_height",    sol::readonly(&Room::cameraHeight),

        // Objects & instances
        "instance_create", &Room::instanceCreateScript,
        "instance_exists", &Room::instanceExistsScript,
        "instance_destroy", &Room::instanceDestroyScript,
        "object_count", &Room::objectCount,
        "object_get", &Room::getObject,
        "object_exists", &Room::objectExists,
        "object_get_list", &Room::objectGetList,
        "object_destroy", &Room::objectDestroy,
        "object_deactivate", &Room::deactivateObject,
        "object_activate", &Room::activateObject,
        "object_activate_region", &Room::activateObjectRegion,

        // Layers
        "tile_layer_get",       &Room::getTileLayer,
        "background_layer_get", &Room::getBackgroundLayer,

        // Collisions
        "instance_place",           &Room::instancePlaceScript,
        "collision_rectangle",      &Room::collisionRectangleScript,
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

    drawAll = lua.script(R"(
return function(drawables, room, alpha)
    local n = #drawables
    for i = 1, n do
        local d = drawables[i]
        if d.draw then d:draw(room, alpha) end
    end
end
    )");
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
                BaseObject* base = objMgr.baseClasses[objectIndex].objectPtr->get();
                auto obj = instanceCreate(x, y, depth, base);
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
            std::unique_ptr<Background> bg = std::make_unique<Background>(lua);

            bg->name = l["name"];
            
            bg->tiledX = l["tiled_x"];
            bg->tiledY = l["tiled_y"];

            bg->xspd = l["speed_x"];
            bg->yspd = l["speed_y"];
            
            bg->x = l["x"];
            bg->y = l["y"];

            bg->visible = l["visible"];
            bg->depth = depth;
            
            if (!l["sprite"].is_null()) {
                bg->spriteIndex = &SpriteManager::get().sprites[l["sprite"]];
            }
            
            auto& col = l["color"].get<std::vector<uint8_t>>();
            if (col.size() == 4) {
                bg->color = *((c_Color*)col.data());
            }
            
            bg->MyReference.id = currentId++;
            backgrounds.push_back(bg.get());
            instances.push_back(std::move(bg));
        }

        if (type == "tiles") {
            std::unique_ptr<Tilemap> map = std::make_unique<Tilemap>(lua);
            
            map->name = l["name"];

            map->tileCountX = l["width"];
            map->tileCountY = l["height"];

            map->visible = l["visible"];
            map->depth = depth;

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
            
            map->tileset = &TilesetManager::get().tilesets[l["tileset"].get<std::string>()];

            map->MyReference.id = currentId++;
            tilemaps.push_back(map.get());
            instances.push_back(std::move(map));
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
    updateQueue();

    for (auto& objUnique : instances) {
        objUnique->runScript("create", this);
    }
    updateQueue();

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
    updateQueue();

    auto start = kvp.find("room_start");
    if (start != kvp.end()) {
        start->second.as<sol::safe_function>()(this);
        updateQueue();
    }

    updateQueue();
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
        if (i->active) {
            i->xPrev = i->x;
            i->yPrev = i->y;
            if (i->incrementImageSpeed) {
                i->imageIndex += (i->imageSpeed * i->imageSpeedMod);
            }
        }
    }

    // Begin Step
    for (auto& instance : instances) {
        if (instance->active) {
            instance->runScript("begin_step", this);
        }
    }
    updateQueue();

    // Step
    for (auto& instance : instances) {
        if (instance->active && instance->stepFunc.has_value()) {
            instance->stepFunc.value()(instance->MyReference, this);
        }
    }
    updateQueue();

    // End Step
    for (auto& instance : instances) {
        if (instance->active) {
            instance->runScript("end_step", this);
        }
    }
    updateQueue();
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
    
    drawables.clear();
    int count = 0;
    for (auto& i : instances) {
        if (i->visible && i->active && dynamic_cast<Tilemap*>(i.get())) {
            count++;
        }
    }
    for (auto& i : instances) {
        if (i->visible && i->active) {
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
        if (d->drawFunc.has_value()) {
            d->drawFunc.value()(d->MyReference, this, alpha);
        }
        else {
            d->draw(this, alpha);
        }
    }
    
    for (auto& d : drawables) {
        d->endDraw(this, alpha);
    }

    target->setView(target->getDefaultView());

    for (auto& d : drawables) {
        if (!d->drawsGui) continue;
        d->drawGui(this, alpha);
    }
}

void Room::setCameraX(float val) {
    cameraX = std::clamp(val, 0.0f, width - cameraWidth);
}

void Room::setCameraY(float val) {
    cameraY = std::clamp(val, 0.0f, height - cameraHeight);
}

void Room::deactivateObject(sol::object object) {
    if (object.is<BaseObject*>()) {
        BaseObject* o = object.as<BaseObject*>();
        for (auto& i : ids) {
            if (i.second->extends(o))
                i.second->active = false;
        }
    }
    else {
        auto& ref = object.as<Object::Reference>();
        auto it = ids.find(ref.id);
        if (it != ids.end()) {
            it->second->active = false;
        }
    }
}

void Room::activateObject(sol::object object) {
    if (object.is<BaseObject*>()) {
        BaseObject* o = object.as<BaseObject*>();
        for (auto& i : ids) {
            if (i.second->extends(o))
                i.second->active = true;
        }
    }
    else {
        auto& ref = object.as<Object::Reference>();
        auto it = ids.find(ref.id);
        if (it != ids.end()) {
            it->second->active = true;
        }
    }
}

void Room::activateObjectRegion(sol::object object, float x1, float y1, float x2, float y2) {
    BaseObject* o = object.as<BaseObject*>();
    sf::FloatRect regionRect = { { x1, y1 }, { x2 - x1, y2 - y1 } };
    for (auto& i : ids) {
        if (!i.second->active && i.second->extends(o)) {
            auto objectRect = i.second->getRectangle();
            if (regionRect.findIntersection(objectRect).has_value()) {
                i.second->active = true;
            }
        }
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
        spr->setColor(c_Color::White);
        float parallax = xspd;
        float parallaxY = yspd;
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