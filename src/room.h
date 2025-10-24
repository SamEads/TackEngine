#pragma once

#include <iostream>
#include "object.h"
#include "collision.h"

class Tileset;
class Tilemap : public Drawable {
public:
    sf::VertexArray va;
    std::vector<unsigned int> tileData;
    int tileCountX, tileCountY;
    std::string name;
    Tileset* tileset;
    void draw(Room* room, float alpha) override;
};

class Background : public Drawable {
public:
    SpriteIndex* spriteIndex;
    float speedX, speedY;
    bool tiledX, tiledY;
    bool offsetX, offsetY;
    float x, y;
    std::string name;
    sf::Color color = sf::Color::White;
    void draw(Room* room, float alpha) override;
};

class RoomReference {
public:
    std::string name;
    std::filesystem::path p;
};

class Room {
public:
    static void initializeLua(sol::state& lua, const std::filesystem::path& assets);

    ObjectId currentId = 0;
    std::vector<std::unique_ptr<Tilemap>> tilemaps;
    std::vector<std::unique_ptr<Background>> backgrounds;
    std::vector<std::unique_ptr<Object>> instances;
    std::vector<std::unique_ptr<Object>> queuedInstances;
    std::vector<Object*> queuedDelete;
    std::unordered_map<ObjectId, Object*> ids;
    sol::state& lua;

    int width, height;
    float cameraX = 0, cameraY = 0;
    float cameraPrevX = 0, cameraPrevY = 0;
    float cameraWidth = 0, cameraHeight = 0;

    Room(sol::state& lua, const RoomReference& room);

    void update();

    void draw(float alpha);

    float getCameraX() {
        return cameraX;
    }

    float getCameraY() {
        return cameraY;
    }

    void setCameraX(float val);

    void setCameraY(float val);

    void addQueue() {
        for (auto& o : queuedInstances) {
            instances.push_back(std::move(o));
        }
        queuedInstances.clear();

        for (auto& o : queuedDelete) {
            auto orig = std::find_if(instances.begin(), instances.end(), [o](const auto& unique) {
                return unique.get() == o;
            });
            if (orig != instances.end()) {
                instances.erase(orig);
            }
        }
        queuedDelete.clear();
    }

    void objectDestroy(std::unique_ptr<Object>& base) {
        for (auto& i : instances) {
            if (i->extends(base.get())) {
                ObjectId id = i->id;
                queuedDelete.push_back(ids[id]);
                ids.erase(id);
            }
        }
    }

    void instanceDestroy(ObjectId id) {
        queuedDelete.push_back(ids[id]);
        ids.erase(id);
    }

    void instanceDestroyScript(sol::object obj) {
        Object* object = nullptr;

        if (obj.is<Object::Reference>()) {
            Object::Reference& ref = obj.as<Object::Reference>();
            object = ref.object.as<Object*>();
        }
        else if (obj.is<Object*>()) {
            object = obj.as<Object*>();
        }
        else {
            return;
        }

        object->runScript("destroy", this);
        queuedDelete.push_back(ids[object->id]);
        ids.erase(object->id);
    }

    int objectCount(Object* baseType) {
        if (baseType == NULL) {
            return false;
        }
        int c = 0;
        for (auto& i : instances) {
            if (i->extends(baseType)) c++;
        }
        return c;
    }

    bool objectExists(Object* baseType) {
        if (baseType == NULL) {
            return false;
        }
        auto it = std::find_if(ids.begin(), ids.end(), [baseType](const auto& o) {
            Object* optr = o.second;
            return optr->extends(baseType);
        });
        if (it != ids.end()) {
            return true;
        }
        return false;
    }

    sol::object getTileLayer(const std::string& key) {
        for (auto& m : tilemaps) {
            if (m->name == key) {
                Tilemap* ptr = m.get();
                return sol::make_object(lua, ptr);
            }
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    sol::object getBackgroundLayer(const std::string& key) {
        for (auto& bg : backgrounds) {
            if (bg->name == key) {
                return sol::make_object(lua, bg.get());
            }
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    std::vector<Object::Reference> objectGetList(Object* baseType);

    sol::object getObject(Object* baseType) {
        if (baseType == NULL) {
            return sol::make_object(lua, sol::lua_nil);
        }
        auto it = std::find_if(ids.begin(), ids.end(), [baseType](const auto& o) {
            Object* optr = o.second;
            return optr->extends(baseType);
        });
        if (it != ids.end()) {
            return sol::make_object(lua, it->second->makeReference());
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    bool instanceExistsScript(sol::object obj) {
        if (obj == sol::lua_nil) {
            return false;
        }
        if (obj.is<Object::Reference>()) {
            Object::Reference* ref = obj.as<Object::Reference*>();
            int refId = ref->id;
            return ids.find(refId) != ids.end();
        }
        return false;
    }

    bool instanceExists(Object::Reference reference) {
        int refId = reference.id;
        return ids.find(refId) != ids.end();
    }

    std::vector<Object::Reference> collisionRectangleList(Object* caller, float x1, float y1, float x2, float y2, std::unique_ptr<Object>& type) {
        if (type == nullptr) {
            return {};
        }

        float width = x2 - x1;
        float height = y2 - y1;
        sf::FloatRect rect = { { x1, y1 }, { width, height } };

        std::vector<Object::Reference> vec {};

        for (auto& i : ids) {
            auto& instance = i.second;
            if (i.second->extends(type.get())) {
                sf::FloatRect otherRect = { { instance->bboxLeft(), instance->bboxTop() }, { 0, 0 } };
                otherRect.size.x = instance->bboxRight() - otherRect.position.x;
                otherRect.size.y = instance->bboxBottom() - otherRect.position.y;
                auto intersection = rect.findIntersection(otherRect);
                if (intersection.has_value()) {
                    vec.push_back(i.second->makeReference());
                }
            }
        }

        return vec;

        /*
        float width = x2 - x1;
        float height = y2 - y1;
        std::vector<sf::Vector2f> callerPts = {
            { x1, y1 }, { x1 + width, y1 },
            { x1 + width, y1 + width }, { x1, y1 + width }
        };

        std::vector<Object::Reference> vec {};

        for (auto& i : ids) {
            if (i.second->extends(type.get())) {
                auto answer = polygonsIntersect(callerPts, i.second->getPoints());
                if (answer.intersect) {
                    vec.push_back(i.second->makeReference());
                }
            }
        }

        return vec;
        */
    }

    sol::object collisionRectangleScript(Object* caller, float x1, float y1, float x2, float y2, sol::object type, sol::variadic_args va) {
        float width = x2 - x1;
        float height = y2 - y1;
        sf::FloatRect rect = { { x1, y1 }, { width, height } };

        // Reference
        if (type.is<Object::Reference>()) {
            Object::Reference& r = type.as<Object::Reference>();

            // Instance doesn't exist, return null
            if (!instanceExists(r)) {
                return sol::make_object(lua, sol::lua_nil);
            }
            
            Object* instance = r.object.as<Object*>();
            sf::FloatRect otherRect = { { instance->bboxLeft(), instance->bboxTop() }, { 0, 0 } };
            otherRect.size.x = instance->bboxRight() - otherRect.position.x;
            otherRect.size.y = instance->bboxBottom() - otherRect.position.y;
            auto intersection = rect.findIntersection(otherRect);
            
            // Intersection vs none
            if (intersection.has_value()) {
                return sol::make_object(lua, r);
            }
            else {
                return sol::make_object(lua, sol::lua_nil);
            }
        }
        else {
            // Object
            auto& baseType = type.as<std::unique_ptr<Object>&>();
            Object* ignore = nullptr;
            if (va.size() > 0) {
                if (va[0].get<bool>() == true) {
                    ignore = caller;
                }
            }
            for (auto& i : ids) {
                auto& instance = i.second;
                if (i.second->extends(baseType.get())) {
                    if (ignore == nullptr || i.second != ignore) {
                        sf::FloatRect otherRect = { { instance->bboxLeft(), instance->bboxTop() }, { 0, 0 } };
                        otherRect.size.x = instance->bboxRight() - otherRect.position.x;
                        otherRect.size.y = instance->bboxBottom() - otherRect.position.y;
                        auto intersection = rect.findIntersection(otherRect);
                        if (intersection.has_value()) {
                            return sol::make_object(lua, i.second->makeReference());
                        }
                    }
                }
            }
    
            return sol::make_object(lua, sol::lua_nil);
        }


        /*
        Precise (old)
        if (type == sol::lua_nil) {
            return sol::make_object(lua, sol::lua_nil);
        }

        float width = x2 - x1;
        float height = y2 - y1;
        std::vector<sf::Vector2f> callerPts = {
            { x1, y1 }, { x1 + width, y1 },
            { x1 + width, y1 + width }, { x1, y1 + width }
        };

        if (type.is<Object::Reference>()) {
            Object::Reference& r = type.as<Object::Reference>();
            if (!instanceExists(r)) {
                return sol::make_object(lua, Object::Reference{ -1, sol::make_object(lua, sol::lua_nil) });
            }
            auto answer = polygonsIntersect(callerPts, r.object.as<Object*>()->getPoints());
            if (answer.intersect) {
                return type;
            }
        }

        Object* ignore = nullptr;
        if (va.size() > 0) {
            if (va[0].get<bool>() == true) {
                ignore = caller;
            }
        }
        auto& baseType = type.as<std::unique_ptr<Object>&>();
        for (auto& i : ids) {
            if (i.second->extends(baseType.get())) {
                if (ignore == nullptr || i.second != ignore) {
                    auto answer = polygonsIntersect(callerPts, i.second->getPoints());
                    if (answer.intersect) {
                        return sol::make_object(lua, i.second->makeReference());
                    }
                }
            }
        }

        return sol::make_object(lua, sol::lua_nil);
        */
    }

    Object::Reference instancePlaceScript(Object* caller, float x, float y, sol::object type) {
        if (type == sol::lua_nil) {
            return Object::Reference { -1, sol::make_object(lua, sol::lua_nil) };
        }

        auto callerPts = caller->getPoints();
        for (auto& p : callerPts) {
            p.x -= caller->x;
            p.y -= caller->y;
            p.x += x;
            p.y += y;
        }

        if (type.is<Object::Reference>()) {
            Object::Reference& r = type.as<Object::Reference>();
            if (!instanceExists(r)) {
                return Object::Reference{ -1, sol::make_object(lua, sol::lua_nil) };
            }
            auto answer = polygonsIntersect(callerPts, r.object.as<Object*>()->getPoints());
            if (answer.intersect) {
                return r;
            }
        }

        auto& baseType = type.as<std::unique_ptr<Object>&>();
        for (auto& i : ids) {
            if (i.second->extends(baseType.get())) {
                auto answer = polygonsIntersect(callerPts, i.second->getPoints());
                if (answer.intersect) {
                    return i.second->makeReference();
                }
            }
        }

        return Object::Reference { -1, sol::make_object(lua, sol::lua_nil) };
    }

    Object::Reference instanceCreateScript(float x, float y, float depth, Object* baseObject) {
        Object* ptr = instanceCreate(x, y, depth, baseObject);

        auto it = ptr->kvp.find("create");
        if (it != ptr->kvp.end()) {
            auto res = it->second.as<sol::safe_function>()(ptr, this);
            if (!res.valid()) {
                sol::error e = res;
                std::cout << e.what() << "\n";
            }
        }
        ptr->xPrev = ptr->x;
        ptr->yPrev = ptr->y;

        return ptr->makeReference();
    }

    Object* instanceCreate(float x, float y, float depth, Object* baseObject) {
        std::unique_ptr<Object> copiedObject = ObjectManager::get().make(lua, baseObject);
        copiedObject->self = baseObject;
        copiedObject->id = currentId++;
        copiedObject->x = x;
        copiedObject->y = y;
        copiedObject->depth = depth;

        Object* ptr = copiedObject.get();

        queuedInstances.push_back(std::move(copiedObject));
        ids[ptr->id] = ptr;

        return ptr;
    }

    std::unordered_map<std::string, sol::object> kvp;
    void setKVP(const std::string& key, sol::main_object obj) {
        auto it = kvp.find(key);
        if (it == kvp.end()) {
            kvp.insert({ key, sol::object(std::move(obj)) });
        }
        else {
            it->second = sol::object(std::move(obj));
        }
    }
    sol::object getKVP(const std::string& ref) {
        auto it = kvp.find(ref);
        if (it == kvp.end()) {
            return sol::lua_nil;
        }
        return it->second;
    }
};