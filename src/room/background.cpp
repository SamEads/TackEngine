#include "room.h"
#include "game.h"

void Background::draw(Room* room, float alpha) {
    float cx = room->renderCameraX;
    float cy = room->renderCameraY;

    float x = cx - 1;
    float y = cy - 1;

    sf::Shader* shader = Game::get().currentShader;
    if (spriteIndex && spriteIndex->sprite) {
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
        sf::RectangleShape rs({ room->view.width + 2, room->view.height + 2 });
        rs.setTexture(&GFX::whiteTexture);
        rs.setFillColor(color);
        rs.setPosition({ x, y });
        Game::get().getRenderTarget()->draw(rs, shader);
    }
}