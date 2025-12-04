#include <fstream>
#include "sprite.h"
#include "vendor/json.hpp"
#include "game.h"
#include "util/mathhelper.h"

static std::tuple<float, float, float, float> GetSpriteUVs(GFX::Sprite* s) {
    sf::Vector2u texSize = s->sprite->getTexture().getSize();
    float left = s->frames[0].frameX;
    float top = s->frames[0].frameY;
    float right = left + s->width;
    float bottom = top + s->height;
    left /= texSize.x;
    right /= texSize.x;
    top /= texSize.y;
    bottom /= texSize.y;
    return { left, top, right, bottom };
}

static std::tuple<float, float> GetSpriteTexelSize(GFX::Sprite* s) {
    sf::Vector2u texSize = s->sprite->getTexture().getSize();
    return { 1.0f / texSize.x, 1.0f / texSize.y };
}

sf::Texture GFX::CreatePaddedTexture(
    const sf::Image& source,
    unsigned int tileWidth,
    unsigned int tileHeight,
    unsigned int frameCountX,
    unsigned int frameCountY,
    unsigned int pad,
    unsigned int offsetX,
    unsigned int offsetY,
    unsigned int separationX,
    unsigned int separationY,
    std::vector<GFX::Sprite::Frame>* outFrameCoords)
{
    unsigned int texWidth = frameCountX * (tileWidth + 2 * pad);
    unsigned int texHeight = frameCountY * (tileHeight + 2 * pad);
    sf::Image padded(sf::Vector2u(texWidth, texHeight), sf::Color::Transparent);

    for (unsigned int y = 0; y < frameCountY; ++y) {
        for (unsigned int x = 0; x < frameCountX; ++x) {
            unsigned int srcX = offsetX + x * (tileWidth + separationX);
            unsigned int srcY = offsetY + y * (tileHeight + separationY);

            unsigned int dstX = pad + x * (tileWidth + 2 * pad);
            unsigned int dstY = pad + y * (tileHeight + 2 * pad);

            bool copied = padded.copy(source, sf::Vector2u(dstX, dstY), sf::IntRect(sf::Vector2i(srcX, srcY), sf::Vector2i(tileWidth, tileHeight)), true);

            for (int i = 0; i < pad; ++i) {
                copied = padded.copy(source, sf::Vector2u(dstX - 1 - i, dstY), sf::IntRect(sf::Vector2i(srcX, srcY), sf::Vector2i(1, tileHeight)), true);
                copied = padded.copy(source, sf::Vector2u(dstX + tileWidth + i, dstY), sf::IntRect(sf::Vector2i(srcX + tileWidth - 1, srcY), sf::Vector2i(1, tileHeight)), true);
            }
            for (int i = 0; i < pad; ++i) {
                copied = padded.copy(source, sf::Vector2u(dstX, dstY - 1 - i), sf::IntRect(sf::Vector2i(srcX, srcY), sf::Vector2i(tileWidth, 1)), true);
                copied = padded.copy(source, sf::Vector2u(dstX, dstY + tileHeight + i), sf::IntRect(sf::Vector2i(srcX, srcY + tileHeight - 1), sf::Vector2i(tileWidth, 1)), true);
            }
            for (int i = 0; i < pad; ++i) {
                for (int j = 0; j < pad; ++j) {
                    // Top-left corner
                    copied = padded.copy(source, sf::Vector2u(dstX - 1 - i, dstY - 1 - j), sf::IntRect(sf::Vector2i(srcX, srcY), sf::Vector2i(1, 1)), true);
                    // Top-right corner
                    copied = padded.copy(source, sf::Vector2u(dstX + tileWidth + i, dstY - 1 - j), sf::IntRect(sf::Vector2i(srcX + tileWidth - 1, srcY), sf::Vector2i(1, 1)), true);
                    // Bottom-left corner
                    copied = padded.copy(source, sf::Vector2u(dstX - 1 - i, dstY + tileHeight + j), sf::IntRect(sf::Vector2i(srcX, srcY + tileHeight - 1), sf::Vector2i(1, 1)), true);
                    // Bottom-right corner
                    copied = padded.copy(source, sf::Vector2u(dstX + tileWidth + i, dstY + tileHeight + j), sf::IntRect(sf::Vector2i(srcX + tileWidth - 1, srcY + tileHeight - 1), sf::Vector2i(1, 1)), true);
                }
            }

            if (outFrameCoords)
                outFrameCoords->push_back(GFX::Sprite::Frame { static_cast<int>(dstX), static_cast<int>(dstY) });
        }
    }

    sf::Texture tex;
    bool loaded = tex.loadFromImage(padded);
    return tex;
}

namespace MetaBuilder {
    template <typename T>
    void Push(const LuaState& L, const std::string& str, float T::* ptr) {

    }

    template <typename T>
    void Push(const LuaState& L, const std::string& str, int T::* ptr) {

    }
}

static void CreateSpriteMetatable(LuaState& L) {
    luaL_newmetatable(L, "SpriteIndex");
        lua_pushcfunction(L, [](lua_State* L) -> int {
            const char* key = lua_tostring(L, 2);
            auto spr = lua_toclassfromref<GFX::Sprite>(L, 1);
            
            if (strcmp(key, "frame_count") == 0) {
                lua_pushinteger(L, spr->frames.size());
                return 1;
            }

            if (strcmp(key, "width") == 0) {
                lua_pushinteger(L, spr->width);
                return 1;
            }

            if (strcmp(key, "height") == 0) {
                lua_pushinteger(L, spr->height);
                return 1;
            }

            if (strcmp(key, "origin_x") == 0) {
                lua_pushinteger(L, spr->originX);
                return 1;
            }

            if (strcmp(key, "origin_y") == 0) {
                lua_pushinteger(L, spr->originY);
                return 1;
            }

            lua_pushvalue(L, 2);    // str
            lua_rawget(L, 1);       // val
            if (!lua_isnil(L, -1)) {
                return 1;
            }

            if (lua_getmetatable(L, 1)) {   // mt
                lua_pushvalue(L, 2);        // key, mt
                lua_rawget(L, -2);          // val, mt
                if (!lua_isnil(L, -1)) {
                    lua_remove(L, -2);
                    return 1;
                }
                else {
                    lua_pop(L, 2);
                }
            }

            lua_pushnil(L);
            return 1;
        });
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, [](lua_State* L) -> int {
            auto uvs = GetSpriteUVs(lua_toclassfromref<GFX::Sprite>(L, 1));
            lua_pushnumber(L, std::get<0>(uvs));
            lua_pushnumber(L, std::get<1>(uvs));
            lua_pushnumber(L, std::get<2>(uvs));
            lua_pushnumber(L, std::get<3>(uvs));
            return 4;
        });
        lua_setfield(L, -2, "get_uvs");

        lua_pushcfunction(L, [](lua_State* L) -> int {
            auto tuple = GetSpriteTexelSize(lua_toclassfromref<GFX::Sprite>(L, 1));
            lua_pushnumber(L, std::get<0>(tuple));
            lua_pushnumber(L, std::get<1>(tuple));
            return 2;
        });
        lua_setfield(L, -2, "get_texel_size");
    lua_pop(L, 1);
}

static void InitializeCoreFunctions(LuaState& L) {
    lua_pushcfunction(L, [](lua_State* L) -> int {
        Game::get().currentRenderer->clear(lua_tocolor(L, 1));
        return 0;
    });
    lua_setfield(L, -2, "clear");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        Game::get().canvasWidth = lua_tonumber(L, 1);
        Game::get().canvasHeight = lua_tonumber(L, 2);
        return 0;
    });
    lua_setfield(L, -2, "set_size");
}

static void InitializeCanvasFunctions(LuaState& L) {
    lua_pushcfunction(L, [](lua_State* L) -> int {
        unsigned int width = static_cast<unsigned int>(lua_tointeger(L, 1));
        unsigned int height = static_cast<unsigned int>(lua_tointeger(L, 2));

        void* mem = lua_newuserdata(L, sizeof(sf::RenderTexture));
        auto canvas = new(mem) sf::RenderTexture(sf::Vector2u { width, height });
        
        return 1;
    });
    lua_setfield(L, -2, "create_canvas");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        if (lua_isnil(L, 1)) {
            Game::get().currentRenderer = nullptr;
        }
        else {
            sf::RenderTexture* target = static_cast<sf::RenderTexture*>(lua_touserdata(L, 1));
            Game::get().currentRenderer = target;
        }
        return 0;
    });
    lua_setfield(L, -2, "set_canvas");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        sf::RenderTexture* target = static_cast<sf::RenderTexture*>(lua_touserdata(L, 1));
        unsigned int width = static_cast<unsigned int>(lua_tointeger(L, 2));
        unsigned int height = static_cast<unsigned int>(lua_tointeger(L, 3));
        bool res = target->resize({ width, height }); //nodisc
        return 0;
    });
    lua_setfield(L, -2, "resize_canvas");

    // sf::RenderTexture* target, float x, float y, float xscale, float yscale, float originx, float originy, float angle
    lua_pushcfunction(L, [](lua_State* L) -> int {
        sf::RenderTexture* target = static_cast<sf::RenderTexture*>(lua_touserdata(L, 1));
        float x = lua_tonumber(L, 2);
        float y = lua_tonumber(L, 3);
        float xscale = lua_tonumber(L, 4);
        float yscale = lua_tonumber(L, 5);
        float originx = lua_tonumber(L, 6);
        float originy = lua_tonumber(L, 7);
        float angle = lua_tonumber(L, 8);

        target->display();
        Game& game = Game::get();
        sf::Sprite s(target->getTexture());
        s.setPosition({ x, y });
        s.setScale({ xscale, yscale });
        s.setOrigin({ originx, originy });
        s.setRotation(sf::degrees(angle));
        game.getRenderTarget()->draw(s, game.currentShader);

        return 0;
    });
    lua_setfield(L, -2, "draw_canvas");
}

static void InitializeDrawFunctions(LuaState& L) {
    lua_pushcfunction(L, [](lua_State* L) -> int {
        GFX::Sprite* spriteIndex = lua_toclassfromref<GFX::Sprite>(L, 1);
        if (spriteIndex == nullptr) {
            return 0;
        }
        else {
            float imageIndex = luaL_checknumber(L, 2);
            float x = luaL_checknumber(L, 3);
            float y = luaL_checknumber(L, 4);

            int frameCount = spriteIndex->frames.size();
            int frameIndex = static_cast<int>(floorf(imageIndex)) % frameCount;

            int texX = spriteIndex->frames[frameIndex].frameX;
            int texY = spriteIndex->frames[frameIndex].frameY;

            spriteIndex->draw(*Game::get().getRenderTarget(), { x, y }, imageIndex);

            return 0;
        }
    });
    lua_setfield(L, -2, "draw_sprite");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        GFX::Sprite* spriteIndex = lua_toclassfromref<GFX::Sprite>(L, 1);
        if (spriteIndex == nullptr) {
            return 0;
        }
        else {
            float imageIndex = luaL_checknumber(L, 2);
            float x = luaL_checknumber(L, 3);
            float y = luaL_checknumber(L, 4);
            float xscale = luaL_checknumber(L, 5);
            float yscale = luaL_checknumber(L, 6);
            float rot = luaL_checknumber(L, 7);
            sf::Color color = lua_tocolor(L, 8);

            int frameCount = spriteIndex->frames.size();
            int frameIndex = static_cast<int>(floorf(imageIndex)) % frameCount;

            int texX = spriteIndex->frames[frameIndex].frameX;
            int texY = spriteIndex->frames[frameIndex].frameY;
            
            spriteIndex->draw(*Game::get().getRenderTarget(), { x, y }, imageIndex, { xscale, yscale }, color, rot);

            return 0;
        }
    });
    lua_setfield(L, -2, "draw_sprite_ext");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        auto spriteIndex = lua_toclassfromref<GFX::Sprite>(L, 1); 
        if (spriteIndex == nullptr) {
            return 0;
        }
        else {
            float imageIndex = luaL_checknumber(L, 2);
            float x = luaL_checknumber(L, 3);
            float y = luaL_checknumber(L, 4);
            float xscale = luaL_checknumber(L, 5);
            float yscale = luaL_checknumber(L, 6);
            float originX = luaL_checknumber(L, 7);
            float originY = luaL_checknumber(L, 8);
            bool keepSpriteOriginPosition = lua_toboolean(L, 9);
            float rot = luaL_checknumber(L, 10);
            sf::Color color = lua_tocolor(L, 11);

            int frameCount = spriteIndex->frames.size();
            int frameIndex = static_cast<int>(floorf(imageIndex)) % frameCount;

            int texX = spriteIndex->frames[frameIndex].frameX;
            int texY = spriteIndex->frames[frameIndex].frameY;
            
            spriteIndex->drawOrigin(*Game::get().getRenderTarget(),
                (keepSpriteOriginPosition) ? sf::Vector2f(x - spriteIndex->originX + originX, y - spriteIndex->originY + originY) : sf::Vector2f(x, y),
                imageIndex, { xscale, yscale }, { originX, originY }, color, rot);

            return 0;
        }
    });
    lua_setfield(L, -2, "draw_sprite_origin");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        float x1 = luaL_checknumber(L, 1);
        float y1 = luaL_checknumber(L, 2);
        float x2 = luaL_checknumber(L, 3);
        float y2 = luaL_checknumber(L, 4);
        sf::Color color = lua_tocolor(L, 5);
        sf::RectangleShape rs({ x2 - x1, y2 - y1 });
        rs.setPosition({ x1, y1 });
        rs.setFillColor(color);
        rs.setTexture(&GFX::whiteTexture);
        rs.setTextureRect({ { 0, 0 }, { 1, 1 } });
        Game& game = Game::get();
        game.getRenderTarget()->draw(rs, game.currentShader);

        return 0;
    });
    lua_setfield(L, -2, "draw_rectangle");

    // float x, float y, float r, ColorTable color
    lua_pushcfunction(L, [](lua_State* L) -> int {
        float x = luaL_checknumber(L, 1);
        float y = luaL_checknumber(L, 2);
        float radius = luaL_checknumber(L, 3);
        sf::Color color = lua_tocolor(L, 4);

        sf::CircleShape cs(radius);
        cs.setPosition({ x, y });
        cs.setOrigin({ radius, radius });
        cs.setFillColor(color);
        cs.setTexture(&GFX::whiteTexture);
        cs.setTextureRect({ { 0, 0 }, { 1, 1 } });

        Game& game = Game::get();
        game.getRenderTarget()->draw(cs, game.currentShader);

        return 0;
    });
    lua_setfield(L, -2, "draw_circle");
}

static void LoadSpritesIntoEnvironment(LuaState& L, const std::filesystem::path& assets) {
    using namespace nlohmann;

    auto paths = {
        assets / "sprites",
        assets / "managed" / "sprites",
    };

    for (const auto& path : paths) {
        for (auto& it : std::filesystem::directory_iterator(path)) {
            std::string identifier = it.path().filename().replace_extension("").string();
            
            if (GFX::sprites.find(identifier) != GFX::sprites.end()) continue;
            if (!it.is_directory() && it.path().extension() != ".png") continue;

            bool isPng = !it.is_directory();
            int pad = 2;
            sf::Image src;
            int frameCount = 0;
            sf::Texture tex;
            std::vector<GFX::Sprite::Frame> frameCoords;
            int frameCountX = -1, frameCountY = -1;

            GFX::sprites[identifier] = std::make_unique<GFX::Sprite>();
            std::unique_ptr<GFX::Sprite>& spr = GFX::sprites[identifier];

            lua_newtable(L); // tbl, te
            luaL_setmetatable(L, "SpriteIndex");
            lua_pushlightuserdata(L, spr.get()); // ud, tbl, te
            lua_setfield(L, -2, "__cpp_ptr"); // tbl, te
            lua_setfield(L, -2, identifier.c_str()); // te

            lua_getfield(L, -1, identifier.c_str()); // tbl, te
            int spriteTable = lua_reference(L, "sprite"); // te

            spr->ref = spriteTable;

            if (!isPng) {
                std::ifstream i(it.path() / "data.json");
                json j = json::parse(i);
                spr->width = j["size"][0].get<int>();
                spr->height = j["size"][1].get<int>();

                bool imageLoaded = src.loadFromFile(it.path() / "frames.png");

                int offByWidth = src.getSize().x / spr->width;
                int offByHeight = src.getSize().y / spr->height;
                frameCountX = offByWidth;
                frameCountY = offByHeight;

                std::vector<float> hitbox = j["hitbox"];
                spr->hitbox.position = { hitbox[0], hitbox[1] };
                spr->hitbox.size.x = hitbox[2] - hitbox[0] + 1.0f;
                spr->hitbox.size.y = hitbox[3] - hitbox[1] + 1.0f;

                spr->originX = j["origin"][0].get<int>();
                spr->originY = j["origin"][1].get<int>();

                tex = GFX::CreatePaddedTexture(src, spr->width, spr->height, frameCountX, frameCountY, pad, 0, 0, 0, 0, &frameCoords);
            }
            else {
                bool imageLoaded = src.loadFromFile(it.path().string());
                auto size = src.getSize();
                bool autoSize = true;
                bool autoHitbox = true;

                auto jPath = it.path().parent_path() / std::string(identifier + ".json");
                if (std::filesystem::exists(jPath)) {
                    std::ifstream i(jPath);
                    json j = json::parse(i);
                    if (j.contains("size") && j["size"].size() == 2) {
                        autoSize = false;
                        spr->width = j["size"][0].get<int>();
                        spr->height = j["size"][1].get<int>();

                        int offByWidth = src.getSize().x / spr->width;
                        int offByHeight = src.getSize().y / spr->height;
                        frameCountX = offByWidth;
                        frameCountY = offByHeight;
                    }
                    if (spr->width == -1 || spr->height == -1) {
                    spr->width = src.getSize().x;
                    spr->height = src.getSize().y;
                        frameCountX = 1;
                        frameCountY = 1;
                    }
                    if (j.contains("hitbox") && j["hitbox"].size() == 4) {
                        autoHitbox = false;
                        std::vector<float> hitbox = j["hitbox"];
                        spr->hitbox.position = { hitbox[0], hitbox[1] };
                        spr->hitbox.size.x = hitbox[2] - hitbox[0] + 1.0f;
                        spr->hitbox.size.y = hitbox[3] - hitbox[1] + 1.0f;
                    }
                    if (j.contains("origin") && j["origin"].size() == 2) {
                        spr->originX = j["origin"][0].get<int>();
                        spr->originY = j["origin"][1].get<int>();
                    }
                }
                    
                if (autoSize) {
                    spr->width = spr->height = size.y;
                    frameCountX = size.x / size.y;
                    if (frameCountX == 0) {
                        frameCountX = 1;
                        spr->width = size.x;
                        spr->height = size.y;
                    }
                }
                if (autoHitbox) {
                    spr->hitbox.position = { 0, 0 };
                    spr->hitbox.size = { static_cast<float>(spr->width), static_cast<float>(spr->height) };
                }

                if (frameCountY == -1) {
                    tex = CreatePaddedTexture(src, spr->width, spr->height, frameCountX, 1, pad, 0, 0, 0, 0, &frameCoords);
                }
                else {
                    tex = CreatePaddedTexture(src, spr->width, spr->height, frameCountX, frameCountY, pad, 0, 0, 0, 0, &frameCoords);
                }
            }

            spr->texture = tex;
            spr->sprite = std::make_unique<sf::Sprite>(spr->texture);
            spr->frames = frameCoords;
        }
    }
}

namespace GFX {
    sf::Texture whiteTexture;
    std::unordered_map<std::string, std::unique_ptr<GFX::Sprite>> sprites;

    void initializeLua(LuaState& L, const std::filesystem::path& assets) {
        sf::Image white(sf::Vector2u { 1, 1 }, sf::Color::White);
        bool loadedWhite = whiteTexture.loadFromImage(white);

        CreateSpriteMetatable(L);
        lua_getglobal(L, ENGINE_ENV);
            LoadSpritesIntoEnvironment(L, assets);
            // Make GFX environment:
            lua_newtable(L);
                InitializeCoreFunctions(L);
                InitializeCanvasFunctions(L);
                InitializeDrawFunctions(L);
            lua_setfield(L, -2, "gfx");
        lua_pop(L, 1);
    }

    void Sprite::drawOrigin(sf::RenderTarget &target, sf::Vector2f position, float frame, sf::Vector2f scale, sf::Vector2f origin, sf::Color color, float rotation) const {
        int frameCount = frames.size();
        int frameIndex = static_cast<int>(frame) % frameCount;
    
        int texX = frames[frameIndex].frameX;
        int texY = frames[frameIndex].frameY;
        sprite->setTextureRect({ { texX, texY }, { width, height } });
    
        sprite->setPosition(position);
        sprite->setOrigin(origin);
        sprite->setScale(scale);
        sprite->setColor(color);
        sprite->setRotation(sf::degrees(rotation));
    
        const sf::Sprite& r = *(sprite.get());
        target.draw(r, Game::get().currentShader);
    }
    
    void Sprite::draw(sf::RenderTarget &target, sf::Vector2f position, float frame, sf::Vector2f scale, sf::Color color, float rotation) const {
        int frameCount = frames.size();
        int frameIndex = static_cast<int>(floorf(frame)) % frameCount;
    
        int texX = frames[frameIndex].frameX;
        int texY = frames[frameIndex].frameY;
        sprite->setTextureRect({ { texX, texY }, { width, height } });
    
        sprite->setPosition(position);
        sprite->setOrigin({ static_cast<float>(originX), static_cast<float>(originY) });
        sprite->setScale(scale);
        sprite->setColor(color);
        sprite->setRotation(sf::degrees(-rotation));
    
        const sf::Sprite& r = *(sprite.get());
        target.draw(r, Game::get().currentShader);
    }
}