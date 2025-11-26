#include <fstream>
#include "room.h"
#include "game.h"
#include "tileset.h"
#include "vendor/json.hpp"

Room::Room(sol::state& lua) : lua(lua) {
    roomReference = nullptr;
    camera.room = this;
}

Room::Room(sol::state& lua, RoomReference* data) : Room(lua) {
    roomReference = data;
}

Background* RoomGetBG(Room* room, const std::string& str) {
    for (auto& it : room->backgrounds) {
        if (it->name == str) {
            return it;
        }
    }
    return nullptr;
}

Background* RoomGetBGList(Room* room) { return nullptr; }

Tilemap* RoomGetTilemap(Room* room, const std::string& str) {
    for (auto& it : room->tilemaps) {
        if (it->name == str) {
            return it;
        }
    }
    return nullptr;
}

Tilemap* RoomGetTilemapList(Room* room) { return nullptr; }

void Room::initializeLua(sol::state &lua, const std::filesystem::path &assets) {
    sol::table engineEnv = lua["TE"];

    Tilemap::initializeLua(lua);
    lua.new_usertype<Background>(
        "Background", sol::no_constructor,
        "visible", &Background::visible,
        "depth", &Background::depth,
        "sprite_index", &Background::spriteIndex,
        "set_color", [&](Background* bg, sol::table color) {
            bg->color = MakeColor(color);
        },
        "get_name", [&](Background* bg) {
            return bg->name;
        },
        "get_color", [&](Background* bg) {
            return lua.create_table_with((float)bg->color.r, (float)bg->color.g, (float)bg->color.b, (float)bg->color.a);
        }
    );

    engineEnv["room_create"] = [&](RoomReference* room) {
        std::unique_ptr<Room> r;
        if (room == nullptr) {
            r = std::make_unique<Room>(lua);
        }
        else {
            r = std::make_unique<Room>(lua, room);
        }
        r->load();
        return std::move(r);
    };

    lua.new_usertype<Camera>(
        "Camera", sol::no_constructor,

        "get_x", &Camera::getX,                 "get_y", &Camera::getY,
        "get_x_previous", &Camera::getXPrev,    "get_y_previous", &Camera::getYPrev,
        "get_width", &Camera::getWidth,         "get_height", &Camera::getHeight,

        "set_x", &Camera::setX,                 "set_y", &Camera::setY,
        "stay_in_bounds", &Camera::stayInBounds,
        
        "teleport", &Camera::teleport
    );

    lua.new_usertype<Room>(
        "Room", sol::no_constructor,

        // Room info
        "view",   &Room::camera,
        "width",    sol::readonly(&Room::width),
        "height",   sol::readonly(&Room::height),

        "background_get",           RoomGetBG,
        "background_list_create",   RoomGetBGList,
        "tilemap_get",              RoomGetTilemap,
        "tilemap_list_create",      RoomGetTilemapList,

        "instance_create",          &Room::instanceCreateScript,
        "instance_exists",          &Room::instanceExistsScript,    // Supports instances & objects as params
        "instance_destroy",         &Room::instanceDestroyScript,   // Supports instances & objects as params
        "instance_get_first",       &Room::getObject,               // Object param only
        "instance_list_create",     &Room::objectGetList,           // Object param only
        "instance_count",           &Room::objectCount,             // Object param only
        "instance_rect",            &Room::collisionRectangleScript,    // Self first argument
        "instances_rect",           &Room::collisionRectangleList,      // Self first argument

        "object_deactivate", &Room::deactivateObject,
        "object_activate", &Room::activateObject,
        "object_activate_region", &Room::activateObjectRegion,

        "step", &Room::timestep,
        "draw", &Room::draw,

        "render_x", &Room::renderCameraX,
        "render_y", &Room::renderCameraY,
        "set_render_position", &Room::setView,

        sol::meta_function::index,      &Room::getKVP,
        sol::meta_function::new_index,  &Room::setKVP
    );

    for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "rooms")) {
        if (!it.is_regular_file() || it.path().extension() != ".bin") {
            continue;
        }

        std::filesystem::path p = it.path();
        std::string identifier = p.filename().replace_extension("").string();

        RoomReference& ref = Game::get().roomReferences[identifier];
        ref.name = identifier;
        ref.p = p;

        engineEnv[identifier] = ref;
    }
}

void Room::load() {
    using namespace nlohmann;

    auto& game = Game::get();
    auto& objMgr = ObjectManager::get();
    auto engineEnv = lua["TE"];

    camera.width = game.canvasWidth;
    camera.height = game.canvasHeight;

    if (roomReference == nullptr) {
        return;
    }

    auto jsonPath = roomReference->p;
    auto binPath = jsonPath.replace_extension(".bin");
    std::ifstream in(binPath, std::ios::binary);

    int settingsWidth, settingsHeight;
    in.read(reinterpret_cast<char*>(&width), sizeof(width));
    in.read(reinterpret_cast<char*>(&height), sizeof(height));

    int layerCount;
    in.read(reinterpret_cast<char*>(&layerCount), sizeof(layerCount));

    auto readstr = [&]() {
        int strLen;
        in.read(reinterpret_cast<char*>(&strLen), sizeof(strLen));

        std::string nameStr;
        nameStr.resize(strLen);

        nameStr[strLen] = '\0';
        in.read(&nameStr[0], strLen);

        return nameStr;
    };

    std::string namestr = readstr();

    for (int i = 0; i < layerCount; ++i) {
        std::string type = readstr();

        std::string name = readstr();

        int depth;
        bool visible;

        in.read(reinterpret_cast<char*>(&depth), sizeof(depth));
        in.read(reinterpret_cast<char*>(&visible), sizeof(visible));

        if (type == "background") {
            std::unique_ptr<Background> bg = std::make_unique<Background>(lua);

            bg->name = name;
            bg->visible = visible;
            bg->depth = depth;

            in.read(reinterpret_cast<char*>(&bg->tiledX), sizeof(bg->tiledX));
            in.read(reinterpret_cast<char*>(&bg->tiledY), sizeof(bg->tiledY));
            in.read(reinterpret_cast<char*>(&bg->xspd), sizeof(bg->xspd));
            in.read(reinterpret_cast<char*>(&bg->yspd), sizeof(bg->yspd));
            in.read(reinterpret_cast<char*>(&bg->x), sizeof(bg->x));
            in.read(reinterpret_cast<char*>(&bg->y), sizeof(bg->y));

            in.read(reinterpret_cast<char*>(&bg->color.r), sizeof(char) * 4);

            bool hasSprite;
            in.read(reinterpret_cast<char*>(&hasSprite), sizeof(hasSprite));
            if (hasSprite) {
                bg->spriteIndex = &SpriteManager::get().sprites[readstr()];
            }

            bg->MyReference.id = currentId++;
            backgrounds.push_back(bg.get());
            bg->vectorPos = instances.size() - 1;
            instances.push_back(std::move(bg));
        }

        else if (type == "tiles") {
            std::unique_ptr<Tilemap> map = std::make_unique<Tilemap>(lua);
            map->name = name;
            map->depth = depth;
            map->visible = visible;

            bool compressed;
            in.read(reinterpret_cast<char*>(&compressed), sizeof(compressed));

            int width, height;
            in.read(reinterpret_cast<char*>(&map->tileCountX), sizeof(map->tileCountX));
            in.read(reinterpret_cast<char*>(&map->tileCountY), sizeof(map->tileCountY));

            if (compressed) {
                size_t tileArrSize;
                in.read(reinterpret_cast<char*>(&tileArrSize), sizeof(tileArrSize));

                int32_t* tiles = new int32_t[tileArrSize];

                in.read((char*)tiles, tileArrSize * sizeof(int32_t));

                auto& decompressed = map->tileData;
		        decompressed.reserve(map->tileCountX * map->tileCountY);

                int size = tileArrSize;
                for (int j = 0; j < size;) {
                    int value = tiles[j++];

                    // start a value train
                    if (value >= 0) {
                        while (true) {
                            // stay in bounds
                            if (j >= size) {
                                break;
                            }

                            int nextValue = tiles[j++];

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
                        if (j >= size) {
                            break;
                        }

                        int repeatValue = tiles[j++];

                        for (int k = 0; k < -value; ++k) {
                            decompressed.push_back(repeatValue);
                        }
                    }
                }

                delete[] tiles;
            }
            else {
                size_t tileArrSize;
                in.read(reinterpret_cast<char*>(&tileArrSize), sizeof(tileArrSize));

                map->tileData.resize(tileArrSize);
                unsigned int* ptr = map->tileData.data();

                in.read((char*)ptr, tileArrSize * sizeof(int32_t));
            }

            std::string tilesetRes = readstr();

            map->tileset = &TilesetManager::get().tilesets[tilesetRes];

            map->MyReference.id = currentId++;
            tilemaps.push_back(map.get());
            map->vectorPos = instances.size() - 1;
            instances.push_back(std::move(map));
        }

        else if (type == "objects") {
            int objectCount;
            in.read(reinterpret_cast<char*>(&objectCount), sizeof(objectCount));

            std::vector<std::string> vec;
            vec.reserve(objectCount);
            for (int i = 0; i < objectCount; ++i) {
                vec.emplace_back(readstr());
            }

            size_t instanceCount;
            in.read(reinterpret_cast<char*>(&instanceCount), sizeof(instanceCount));

            for (int i = 0; i < instanceCount; ++i) {
                int objNamePos;
                in.read(reinterpret_cast<char*>(&objNamePos), sizeof(objNamePos));
                std::string& name = vec[objNamePos];

                float x;
                in.read(reinterpret_cast<char*>(&x), sizeof(x));

                float y;
                in.read(reinterpret_cast<char*>(&y), sizeof(y));

                BaseObject* base = objMgr.gmlObjects[name];
                
                auto obj = instanceCreate(x, y, depth, base);

                bool readAdvanced = false;
                in.read(reinterpret_cast<char*>(&readAdvanced), sizeof(readAdvanced));
                if (readAdvanced) {
                    float& rotation = obj->imageAngle;
                    in.read(reinterpret_cast<char*>(&rotation), sizeof(rotation));

                    float& imageIndex = obj->imageIndex;
                    in.read(reinterpret_cast<char*>(&imageIndex), sizeof(imageIndex));

                    float& imageSpeed = obj->imageSpeedMod;
                    in.read(reinterpret_cast<char*>(&imageSpeed), sizeof(imageSpeed));

                    float& scaleX = obj->xScale;
                    in.read(reinterpret_cast<char*>(&scaleX), sizeof(scaleX));

                    float& scaleY = obj->yScale;
                    in.read(reinterpret_cast<char*>(&scaleY), sizeof(scaleY));

                    uint8_t col[4];
                    in.read((char*)col, sizeof(uint8_t) * 4);
                }

                int propertyCount;
                in.read(reinterpret_cast<char*>(&propertyCount), sizeof(propertyCount));

                if (propertyCount > 0) {
                    if (obj->kvp.find("properties") == obj->kvp.end()) {
                        obj->setDyn("properties", sol::table(lua, sol::create));
                    }

                    sol::table props = obj->getDyn("properties");
                    for (int j = 0; j < propertyCount; ++j) {
                        std::string key = readstr();

                        uint8_t type;
                        in.read(reinterpret_cast<char*>(&type), sizeof(type));

                        if (type == 0) { // float
                            float val;
                            in.read(reinterpret_cast<char*>(&val), sizeof(val));
                            props[key] = sol::make_object(lua, val);
                        }
                        else if (type == 1) { // int
                            int val;
                            in.read(reinterpret_cast<char*>(&val), sizeof(val));
                            props[key] = sol::make_object(lua, val);
                        }
                        else if (type == 2) { // bool
                            bool val;
                            in.read(reinterpret_cast<char*>(&val), sizeof(val));
                            props[key] = sol::make_object(lua, val);
                        }
                        else { // other
                            std::string val = readstr();
                            if (engineEnv[val] != sol::lua_nil) {
                                props[key] = engineEnv[val];
                            }
                            else {
                                props[key] = sol::make_object(lua, val);
                            }
                        }
                    }
                }
            }
        }
    }

    createAndRoomStartEvents();
}

// Room ->      "Create"
// Instances -> "Create"
// Room ->      Creation Code
// Room ->      "Room Start"
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

    auto start = kvp.find("room_start");
    if (start != kvp.end()) {
        start->second.as<sol::safe_function>()(this);
        updateQueue();
    }

    for (auto& objUnique : instances) {
        objUnique->runScript("room_start", this);
    }
    updateQueue();

    camera.xPrev = camera.x;
    camera.yPrev = camera.y;
}

void Room::timestep() {
    camera.xPrev = camera.x;
    camera.yPrev = camera.y;

    for (auto& i : instances) {
        if (i->active) {
            i->xPrev = i->x;
            i->yPrev = i->y;
            i->xPrevRender = i->x;
            i->yPrevRender = i->y;
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
        if (instance->active) {
            instance->runScript("step", this);
        }
        /*
        if (instance->active && instance->stepFunc.has_value()) {
            instance->stepFunc.value()(instance->MyReference, this);
        }
        */
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
    auto target = Game::get().getRenderTarget();

    sf::View view({ { 0, 0 }, { camera.width, camera.height } });

    view.setCenter({ cx + camera.width / 2.0f, cy + camera.height / 2.0f });
    target->setView(view);

    renderCameraX = cx;
    renderCameraY = cy;
}

void Room::draw(float alpha) {
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
        /*if (d->drawFunc.has_value()) {
            d->drawFunc.value()(d->MyReference, this, alpha);
        }
        else*/ {
            d->draw(this, alpha);
        }
    }
    
    for (auto& d : drawables) {
        d->endDraw(this, alpha);
    }

    auto target = Game::get().getRenderTarget();
    target->setView(target->getDefaultView());

    for (auto& d : drawables) {
        if (!d->drawsGui) continue;
        d->drawGui(this, alpha);
    }
}

void Room::deactivateObject(sol::object object) {
    if (object.is<BaseObject*>()) {
        BaseObject* o = object.as<BaseObject*>();
        for (auto& i : ids) {
            if (i.second->extends(o)) {
                i.second->active = false;
            }
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

    sf::Shader* shader = Game::get().currentShader;
    if (spriteIndex) {
        sf::Sprite* spr = spriteIndex->sprite.get();
        spr->setScale({ 1, 1 });
        spr->setOrigin({ 0, 0 });
        spr->setColor(color);
        spr->setRotation(sf::degrees(0));
        spr->setColor({ 255, 255, 255, 255 });
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
                Game::get().getRenderTarget()->draw(*spr, shader);
            }
        }
    }
    else {
        sf::RectangleShape rs({ room->camera.width + 2, room->camera.height + 2 });
        rs.setTexture(&SpriteManager::get().whiteTexture);
        rs.setFillColor(color);
        rs.setPosition({ x, y });
        Game::get().getRenderTarget()->draw(rs, shader);
    }
}