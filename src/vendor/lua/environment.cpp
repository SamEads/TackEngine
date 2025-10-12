#ifdef meow

#include "environment.h"

#include "../../texturemanager.h"
#include "../../object.h"
#include "../../client/game.h"
#include "../../objectregistry.h"
#include "../../server/roomserver.h"
#include "../../cutscene.h"
#include "../../objects/solidobject.h"

template <typename T>
void addHardType(sol::state& lua, std::string identifier, std::string extensionOf, std::string asset, bool isLocal) {
    if (lua["object"][identifier] != sol::lua_nil) {
        return;
    }
    if (ObjectRegistry::get().getBaseClasses(isLocal).find(identifier) != ObjectRegistry::get().getBaseClasses(isLocal).end()) {
        return;
    }

    auto fakeClass = std::make_unique<T>(lua);
    fakeClass->self = fakeClass.get();
    if (!extensionOf.empty()) {
        fakeClass->parent = lua["object"][extensionOf].get<ScriptObject*>();
        for (std::pair p : fakeClass->parent->kvp) {
            fakeClass->kvp.insert(p);
        }
    }

    fakeClass->luaName = identifier;
    lua["object"].get<sol::table>()[identifier] = std::move(fakeClass);

    auto& selfobj = lua["object"][identifier].get<T>();
    auto selfptr = &selfobj;

    // Creation instructions
    ObjectRegistry::ScriptedInfo i = { lua["object"][identifier], [](std::unique_ptr<ScriptObject>& original) {
        T* casted = static_cast<T*>(original.get());
        return std::make_shared<T>(*casted);
    }};

    // Add to list
    ObjectRegistry::get().assetToIdentifier[asset] = identifier;
    ObjectRegistry::get().getBaseClasses(isLocal).insert({ identifier, i });
}

void initLua(sol::state& lua, bool isServer) {
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::package, sol::lib::math, sol::lib::table);
    lua["package"]["path"] = "assets/scripts/?.lua";

    lua.create_named_table("object");

    lua.new_usertype<SpriteData>(
        sol::no_constructor,

        "width", sol::property(&SpriteData::getWidth, &SpriteData::trySet),
        "height", sol::property(&SpriteData::getHeight, &SpriteData::trySet),
        "origin_x", sol::property(&SpriteData::getOriginX, &SpriteData::trySet),
        "origin_y", sol::property(&SpriteData::getOriginY, &SpriteData::trySet),
        "frame_count", sol::property(&SpriteData::getFrameCount, &SpriteData::trySet),
        "string", sol::readonly(&SpriteData::string)
    );

    lua.new_usertype<ScriptObject::Reference>(
        sol::no_constructor,
        //"id", sol::readonly(&ScriptObject::Reference::id),
        "obj", sol::readonly(&ScriptObject::Reference::object)
    );

    if (isServer) {
        sol::basic_usertype<ScriptObject, sol::reference> object = lua.new_usertype<ScriptObject>(
            "ScriptObject", sol::no_constructor,

            "x", &ScriptObject::x,
            "y", &ScriptObject::y,
            "hspeed", &ScriptObject::xspd,
            "vspeed", &ScriptObject::yspd,
            "speed", & ScriptObject::speed,
            "sprite_index", sol::property(&ScriptObject::setSprite, &ScriptObject::getSprite),
            "image_index", &ScriptObject::imageIndex,
            "image_xscale", &ScriptObject::scaleX,
            "image_yscale", &ScriptObject::scaleY,
            "image_speed", &ScriptObject::imageSpeed,
            "image_angle", &ScriptObject::rotation,
            "xprevious", sol::readonly(&ScriptObject::xLast),
            "yprevious", sol::readonly(&ScriptObject::yLast),
            "bbox_left", sol::property(&ScriptObject::bboxLeft, &ScriptObject::trySet),
            "bbox_right", sol::property(&ScriptObject::bboxRight, &ScriptObject::trySet),
            "bbox_bottom", sol::property(&ScriptObject::bboxBottom, &ScriptObject::trySet),
            "bbox_top", sol::property(&ScriptObject::bboxTop, &ScriptObject::trySet),
            "group", sol::readonly(&ScriptObject::group),
            "controller", sol::property(&ScriptObject::getController, &ScriptObject::setController),
            "in_control", sol::property(&ScriptObject::getInControl, &ScriptObject::setInControl),
            "id", sol::property(&ScriptObject::makeReference, &ScriptObject::trySet),
            // "get_id", &ScriptObject::makeReference,

            "depth", &ScriptObject::depth,
            "visible", &ScriptObject::visible,

            "extends", &ScriptObject::extends,
            "sync", &ScriptObject::sync,
            "setup_hitbox", &ScriptObject::setupHitbox,
            "collide", &ScriptObject::collide,
            "use_count", &ScriptObject::getUseCount,

            sol::meta_function::index, &ScriptObject::getDyn,
            sol::meta_function::new_index, &ScriptObject::setDyn
        );
    }

    if (!isServer) {
        lua.new_usertype<ScriptObject>(
            "ScriptObject", sol::no_constructor,

            "x", &ScriptObject::x,
            "y", &ScriptObject::y,
            "hspeed", &ScriptObject::xspd,
            "vspeed", & ScriptObject::yspd,
            "speed", &ScriptObject::speed,
            "sprite_index", sol::property(&ScriptObject::setSprite, &ScriptObject::getSprite),
            "image_index", &ScriptObject::imageIndex,
            "image_xscale", sol::readonly(&ScriptObject::scaleX),
            "image_yscale", sol::readonly(&ScriptObject::scaleY),
            "image_speed", &ScriptObject::imageSpeed,
            "image_angle", &ScriptObject::rotation,
            "xprevious", sol::readonly(&ScriptObject::xLast),
            "yprevious", sol::readonly(&ScriptObject::yLast),
            "bbox_left", sol::property(&ScriptObject::bboxLeft, &ScriptObject::trySet),
            "bbox_right", sol::property(&ScriptObject::bboxRight, &ScriptObject::trySet),
            "bbox_bottom", sol::property(&ScriptObject::bboxBottom, &ScriptObject::trySet),
            "bbox_top", sol::property(&ScriptObject::bboxTop, &ScriptObject::trySet),
            "group", sol::readonly(&ScriptObject::group),
            "in_control", sol::readonly(&ScriptObject::inControl),
            "id", sol::property(&ScriptObject::makeReference, &ScriptObject::trySet),
            // "get_id", &ScriptObject::makeReference,

            "depth", &ScriptObject::depth,
            "visible", sol::readonly(&ScriptObject::visible),

            "extends", &ScriptObject::extends,
            "collide", &ScriptObject::collide,

            sol::meta_function::index, &ScriptObject::getDyn,
            sol::meta_function::new_index, &ScriptObject::setDyn
        );
    }

    lua.new_usertype<PlayerConnection>(
        sol::no_constructor,
        "warp_id", sol::readonly(&PlayerConnection::warpId),
        "id", sol::property(&PlayerConnection::getIdScript, &PlayerConnection::trySet),
        "switch_rooms", &PlayerConnection::switchRoomsScript,
        "camera_follow", &PlayerConnection::cameraFollow,
        "play_music", &PlayerConnection::playMusic,
        "play_sound", &PlayerConnection::playSound,
        "fade_music_to", &PlayerConnection::fadeMusic,

        sol::meta_function::index, &PlayerConnection::getDyn,
        sol::meta_function::new_index, &PlayerConnection::setDyn
    );

    sol::table gfx = lua.create_named_table("gfx");
    gfx["get_sprite"] = [&](std::string spriteIndex, float imageIndex, float x, float y) {
        auto it = textureManager.sprites.find(spriteIndex);
        if (it != textureManager.sprites.end()) {
            return (SpriteData*)&it->second;
        }
        else {
            return (SpriteData*)NULL;
        }
    };
#ifndef SERVER_MODE
    if (!isServer) {
        sol::table sound = lua.create_named_table("sound");
        sound["play"] = [&](const std::string& soundName) {
            soundManager.play(soundName + ".wav");
        };

        gfx["font_set"] = [&](Font font) {
            auto it = globals.fonts.find(font);
            if (it != globals.fonts.end()) {
                game.currentFont = &globals.fonts[font];
            }
        };

        gfx["text_get_width"] = [&](const std::string& str, int size) {
            int largestWidth = 0;
            int currentWidth = 0;
            sf::Text t(*game.currentFont, str, size);
            return t.getLocalBounds().size.x;
        };

        gfx["draw_text_mono"] = [&](float x, float y, int spacing, const std::string& str, int size, sol::table colors) {
            sf::VertexArray arr(sf::PrimitiveType::Triangles);
            uint8_t r = colors.get<int>(1);
            uint8_t g = colors.get<int>(2);
            uint8_t b = colors.get<int>(3);
            uint8_t a = colors.get<int>(4);
            auto& font = *game.currentFont;
            int pos = 0;
            int line = 1;
            auto& tex = font.getTexture(size);
            float lineSpacing = font.getLineSpacing(32);
            for (auto& c : str) {
                if (c == '\n') {
                    pos = 0;
                    line++;
                }
                else if (c == ' ') {
                    pos++;
                }
                else {
                    addLetterQuad(c, &font, size, arr, { x + static_cast<float>(pos * spacing), y + (line * lineSpacing) }, { r, g, b, a }, { r, g, b, a });
                    pos++;
                }
            }
            game.currentRenderer->draw(arr, &tex);
        },

        gfx["draw_text"] = [&](float x, float y, const std::string& str, int size, sol::table colors) {
            sf::Text t(*game.currentFont, str, size);
            uint8_t r = colors.get<int>(1);
            uint8_t g = colors.get<int>(2);
            uint8_t b = colors.get<int>(3);
            uint8_t a = colors.get<int>(4);
            t.setFillColor({ r, g, b, a });
            t.setPosition({ x, y });
            game.currentRenderer->draw(t);
        };
        gfx["draw_circle"] = [&](float x, float y, float radius, sol::table colors) {
            sf::CircleShape cs(radius);
            cs.setPosition({ x, y });
            cs.setOrigin({ radius, radius });
            uint8_t r = colors.get<int>(1);
            uint8_t g = colors.get<int>(2);
            uint8_t b = colors.get<int>(3);
            uint8_t a = colors.get<int>(4);
            cs.setFillColor({ r, g, b, a });
            game.currentRenderer->draw(cs);
        };
        gfx["draw_rectangle"] = [&](float x1, float y1, float x2, float y2, sol::table color) {
            sf::RectangleShape rs({ x2 - x1, y2 - y1 });
            rs.setPosition({ x1, y1 });
            uint8_t r = color.get<int>(1);
            uint8_t g = color.get<int>(2);
            uint8_t b = color.get<int>(3);
            uint8_t a = color.get<int>(4);
            rs.setFillColor({ r, g, b, a });
            game.currentRenderer->draw(rs);
        };
        gfx["draw_rectangle_size"] = [&](float x, float y, float width, float height, sol::table colors) {
            sf::RectangleShape rs({ width, height });
            rs.setPosition({ x, y });
            uint8_t r = colors.get<int>(1);
            uint8_t g = colors.get<int>(2);
            uint8_t b = colors.get<int>(3);
            uint8_t a = colors.get<int>(4);
            rs.setFillColor({ r, g, b, a });
            game.currentRenderer->draw(rs);
        };
        gfx["draw_menu_box"] = [&](float image_index, float x1, float y1, float x2, float y2, float scale) {
            MenuBox::draw(*game.currentRenderer, x1, y1, x2, y2, game.time * 0.25f);
        };
        gfx["draw_sprite"] = [&](sol::object spriteIndex, float imageIndex, float x, float y) {
            SpriteData* useData = NULL;
            if (spriteIndex.is<std::string>()) {
                auto it = textureManager.sprites.find(spriteIndex.as<std::string>());
                if (it != textureManager.sprites.end()) {
                    useData = &it->second;
                }
            }
            if (!useData) {
                useData = spriteIndex.as<SpriteData*>();
            }
            useData->drawBasic(*game.currentRenderer, sf::Vector2f{ x, y }, imageIndex, { 1.0f, 1.0f }, sf::Color::White);
        };
        gfx["draw_sprite_ext"] = [&](sol::object spriteIndex, float imageIndex, float x, float y, float xscale, float yscale, sol::variadic_args va) {
            float rotation = 0;
            sf::Color c = sf::Color::White;
            if (va.size() > 0) {
                rotation = va[0].as<float>();
            }
            if (va.size() > 1) {
                sol::table color = va[1].as<sol::table>();
                uint8_t r = color.get<int>(1);
                uint8_t g = color.get<int>(2);
                uint8_t b = color.get<int>(3);
                uint8_t a = color.get<int>(4);
                c = { r, g, b, a };
            }
            if (spriteIndex.is<std::string>()) {
                auto it = textureManager.sprites.find(spriteIndex.as<std::string>());
                if (it != textureManager.sprites.end()) {
                    it->second.drawBasic(*game.currentRenderer, sf::Vector2f { x, y }, imageIndex, { xscale, yscale }, c, rotation);
                }
            }
            else if (spriteIndex) {
                spriteIndex.as<SpriteData*>()->drawBasic(*game.currentRenderer, sf::Vector2f { x, y }, imageIndex, { xscale, yscale }, c, rotation);
            }
        };
    }
#endif


    lua.set_function("object_create", [&lua, isServer](sol::table info) {
        std::string identifier = info["name"];
        auto& objectRegistry = ObjectRegistry::get();
        auto& baseClasses = objectRegistry.getBaseClasses(!isServer);

        if (baseClasses.find(identifier) != baseClasses.end()) {
            return sol::object(sol::lua_nil);
        }

        sol::object toExtend = info["extends"];

        std::unique_ptr<ScriptObject> newObject = std::make_unique<ScriptObject>(lua);

        if (toExtend != sol::lua_nil) {
            std::unique_ptr<ScriptObject>& original = toExtend.as<std::unique_ptr<ScriptObject>&>();
            newObject->parent = original.get();

            for (std::pair p : original->kvp) {
                newObject->kvp.insert(p);
            }
        }

        lua["object"][identifier] = std::move(newObject);
        std::unique_ptr<ScriptObject>& memPtr = lua["object"][identifier].get<std::unique_ptr<ScriptObject>&>();

        if (info["asset"] != sol::lua_nil) {
            std::string assetId = info["asset"];
            memPtr->serverName = assetId;
            objectRegistry.assetToIdentifier[assetId] = identifier;
        }

        baseClasses[identifier] = ObjectRegistry::ScriptedInfo {
            lua["object"][identifier],
            [](std::unique_ptr<ScriptObject>& original) { return std::make_shared<ScriptObject>(*(original.get())); }
        };


        sol::object solObject = lua["object"][identifier];
        memPtr->self = memPtr.get();
        memPtr->luaName = identifier;
        return solObject;
    });

    lua.new_usertype<ReadonlyScriptObject>(
        sol::no_constructor,

        sol::meta_function::index, &ReadonlyScriptObject::getDyn,
        sol::meta_function::new_index, &ReadonlyScriptObject::setDyn
    );

    lua.new_usertype<Cutscene>(
        sol::no_constructor,

        "add_connection", &Cutscene::addConnection,

        "updates", &Cutscene::updates,
        "commands", &Cutscene::commands,
        "wait", &Cutscene::wait,
        "wait_talk", &Cutscene::waitTalk,
        "talk", &Cutscene::talk,
        "text", &Cutscene::text,

        "set_actor", &Cutscene::setActor,
        "walk", &Cutscene::walk,
        "walk_to", &Cutscene::walkTo,

        sol::meta_function::index, &Cutscene::getDyn,
        sol::meta_function::new_index, &Cutscene::setDyn,

        sol::base_classes, sol::bases<ScriptObject>()
    );

    // lua.script("object_create({ name = \"o_Solid\", asset = \"solid\", readonly = true })");
    addHardType<Cutscene>(lua, "Cutscene", "", "cutscene", !isServer);
    addHardType<SolidObject>(lua, "Solid", "", "solid", !isServer);
    addHardType<SlopeObject>(lua, "Incline", "Solid", "incline", !isServer);

    lua.new_usertype<Room>(
        sol::no_constructor,
        "instance_create", &Room::instanceCreate,
        "cutscene_create", &Room::cutsceneCreate,
        "instance_place", &Room::instancePlaceScript,
        "instance_exists", &Room::instanceExists,
        "object_exists", &Room::objectExists,
        "instance_destroy", &Room::instanceDestroy,
        "id", sol::readonly(&Room::id)
    );

    if (!isServer) {
#ifndef SERVER_MODE
        lua.new_usertype<RoomClient> (
            sol::no_constructor,
            "camera_get_size_x", &RoomClient::cameraGetSizeX,
            "camera_get_size_y", &RoomClient::cameraGetSizeY,
            "camera_set_x", &RoomClient::cameraSetX,
            "camera_set_y", &RoomClient::cameraSetY,

            sol::base_classes, sol::bases<Room>()
        );
#endif
    }
    else {
        lua.new_usertype<RoomServer>(
            sol::no_constructor,
            "instance_create_local", &RoomServer::instanceCreateLocal,
            "get_object_list", &RoomServer::getObjects,
            sol::base_classes, sol::bases<Room>()
        );
    }

    lua["math"]["lerp"] = [](sol::variadic_args va) {
        return lerp(va[0], va[1], va[2], (va.size() < 4 ? EaseType::NONE : va[3].get<EaseType>()));
    };
    lua["math"]["sign"] = [](float value) { return (value < 0) ? -1 : ((value > 0) ? 1 : 0); };
    lua["table"]["erase"] = [](float value) {
        return (value < 0) ? -1 : ((value > 0) ? 1 : 0);
    };

    if (isServer) {
        lua.set_function("is_server", []() { return true; });
        lua.set_function("is_client", []() { return false; });
    }
    else {
        lua.set_function("is_client", []() { return true; });
        lua.set_function("is_server", []() { return false; });
    }

    // ENUMS
    lua.new_enum(
        "Direction",
        "left",         FaceDirection::LEFT,
        "right",        FaceDirection::RIGHT,
        "up",           FaceDirection::UP,
        "down",         FaceDirection::DOWN
    );

    lua.new_enum(
        "EaseType",
        "none",         EaseType::NONE,
        "in_cubic",     EaseType::EASE_IN_CUBIC,
        "out_cubic",    EaseType::EASE_OUT_CUBIC,
        "in_back",      EaseType::EASE_IN_BACK,
        "out_back",     EaseType::EASE_OUT_BACK,
        "in_out_back",  EaseType::EASE_IN_OUT_CUBIC,
        "out_bounce",   EaseType::EASE_OUT_BOUNCE,
        "out_circ",     EaseType::EASE_OUT_CIRC,
        "out_quart",    EaseType::EASE_OUT_QUART
    );

    lua.new_enum(
        "Font",
        "main", Font::MAIN,
        "dotumche", Font::DOTUMCHE,
        "retro", Font::RETRO
    );

    // gui
    if (!isServer) {
#ifndef SERVER_MODE
        lua.set_function("open_gui", [&lua]() {
            game.client->addPacket(WritePacket(Network::PacketType::GUI_REQUEST));
        });
        lua.set_function("close_gui", [&lua]() {
            game.client->addPacket(WritePacket(Network::PacketType::GUI_CLOSE));
        });
#endif
    }

    // input
    if (!isServer) {
#ifndef SERVER_MODE
        enum class Button : int {
            LEFT = 0,
            RIGHT,
            UP,
            DOWN,
            Z,
            X,
            C,
            BUTTON_COUNT
        };
        lua.new_enum(
            "Button",
            "left", Button::LEFT,
            "right", Button::RIGHT,
            "up", Button::UP,
            "down", Button::DOWN,
            "z", Button::Z,
            "x", Button::X,
            "c", Button::C
        );
        std::unordered_map<int, sf::Keyboard::Scancode> buttonMap = {
            { (int)Button::LEFT, sf::Keyboard::Scancode::Left },
            { (int)Button::RIGHT, sf::Keyboard::Scancode::Right },
            { (int)Button::UP, sf::Keyboard::Scancode::Up },
            { (int)Button::DOWN, sf::Keyboard::Scancode::Down },
            { (int)Button::Z, sf::Keyboard::Scancode::Z },
            { (int)Button::X, sf::Keyboard::Scancode::X },
            { (int)Button::C, sf::Keyboard::Scancode::C }
        };
        lua.create_named_table(
            "controller",
            "held", [buttonMap](Button button) {
                sf::Keyboard::Scancode k = buttonMap.find(static_cast<int>(button))->second;
                return keys.held(k);
            },
            "pressed", [buttonMap](Button button) {
                sf::Keyboard::Scancode k = buttonMap.find(static_cast<int>(button))->second;
                return keys.pressed(k);
            },
            "released", [buttonMap](Button button) {
                sf::Keyboard::Scancode k = buttonMap.find(static_cast<int>(button))->second;
                return keys.released(k);
            }
        );
#endif
    }
}

#endif