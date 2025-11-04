#pragma once

#include <iostream>
#include "object.h"
#include "collision.h"
#include "tilemap.h"

class Background : public Object {
public:
    std::string name;
    bool tiledX, tiledY;
    bool offsetX, offsetY;
    c_Color color = c_Color { 255, 255, 255, 255 };

    Background(sol::state& lua) : Object(lua) {}
    void draw(Room* room, float alpha) override;
};

class RoomReference {
public:
    std::string name;
    std::filesystem::path p;
};

class Room {
private:
    const RoomReference* roomReference;
    void createAndRoomStartEvents();
public:
    static void initializeLua(sol::state& lua, const std::filesystem::path& assets);

    ObjectId currentId = 0;
    
    std::vector<std::unique_ptr<Object>> instances;
    std::vector<Background*> backgrounds;
    std::vector<Tilemap*> tilemaps;
    std::vector<Object*> drawables;

    std::vector<std::unique_ptr<Object>> addQueue;
    std::vector<ObjectId> deleteQueue;
    
    std::unordered_map<ObjectId, Object*> ids;
    sol::state& lua;

    int minReserved = 0;
    int width, height;
    float cameraX = 0, cameraY = 0;
    float cameraPrevX = 0, cameraPrevY = 0;
    float renderCameraX = 0, renderCameraY = 0;
    float cameraWidth = 0, cameraHeight = 0;

    Room(sol::state& lua, const RoomReference& room);

    void load();

    void update();

    void draw(float alpha);

    void setView(float cx, float cy);

    float getCameraX() const { return cameraX; }
    float getCameraY() const { return cameraY; }

    void setCameraX(float val);

    void setCameraY(float val);

    void deactivateObject(sol::object object);
    void activateObject(sol::object object);
    void activateObjectRegion(sol::object object, float x1, float y1, float x2, float y2);

    void updateQueue() {
        // Add queued objects
        for (auto& o : addQueue) {
            instances.push_back(std::move(o));
        }

        // Delete objects queued for deletion
        for (auto& o : deleteQueue) {
            auto orig = std::find_if(instances.begin(), instances.end(), [o](const auto& unique) {
                return unique->MyReference.id == o;
            });
            if (orig != instances.end()) {
                instances.erase(orig);
            }
        }

        addQueue.clear();
        deleteQueue.clear();
    }

    void objectDestroy(std::unique_ptr<BaseObject>& base) {
        for (auto& i : instances) {
            if (i->extends(base.get())) {
                ObjectId id = i->MyReference.id;
                deleteQueue.push_back(id);
                ids.erase(id);
            }
        }
    }

    void instanceDestroy(ObjectId id) {
        deleteQueue.push_back(id);
        ids.erase(id);
    }

    void instanceDestroyScript(sol::object obj) {
        Object* object = nullptr;

        if (obj.is<Object::Reference>()) {
            Object::Reference& ref = obj.as<Object::Reference>();
            object = ref.object;
        }
        else if (obj.is<Object*>()) {
            object = obj.as<Object*>();
        }
        else {
            return;
        }

        object->runScript("destroy", this);
        deleteQueue.push_back(object->MyReference.id);
        ids.erase(object->MyReference.id);
    }

    int objectCount(BaseObject* baseType) {
        if (baseType == NULL) {
            return false;
        }
        int c = 0;
        for (auto& i : instances) {
            if (i->extends(baseType)) c++;
        }
        return c;
    }

    bool objectExists(BaseObject* baseType) {
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
                return sol::make_object(lua, m);
            }
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    sol::object getBackgroundLayer(const std::string& key) {
        for (auto& bg : backgrounds) {
            if (bg->name == key) {
                return sol::make_object(lua, bg);
            }
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    std::vector<Object::Reference> objectGetList(BaseObject* baseType);

    sol::object getObject(BaseObject* baseType) {
        if (baseType == NULL) {
            return sol::make_object(lua, sol::lua_nil);
        }
        auto it = std::find_if(ids.begin(), ids.end(), [baseType](const auto& o) {
            Object* optr = o.second;
            return optr->extends(baseType);
        });
        if (it != ids.end()) {
            return sol::make_object(lua, it->second->MyReference);
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

    std::vector<Object::Reference> collisionRectangleList(const Object::Reference& caller, float x1, float y1, float x2, float y2, std::unique_ptr<BaseObject>& type) {
        if (type == nullptr) {
            return {};
        }

        float width = x2 - x1;
        float height = y2 - y1;
        sf::FloatRect rect = { { x1, y1 }, { width, height } };

        std::vector<Object::Reference> vec {};

        for (auto& i : ids) {
            auto& instance = i.second;
            if (i.second->active && i.second->extends(type.get())) {
                sf::FloatRect otherRect = { { instance->bboxLeft(), instance->bboxTop() }, { 0, 0 } };
                otherRect.size.x = instance->bboxRight() - otherRect.position.x;
                otherRect.size.y = instance->bboxBottom() - otherRect.position.y;
                auto intersection = rect.findIntersection(otherRect);
                if (intersection.has_value()) {
                    vec.push_back(i.second->MyReference);
                }
            }
        }

        return vec;
    }

    sol::object collisionRectangleScript(const Object::Reference& caller, float x1, float y1, float x2, float y2, sol::object type, sol::variadic_args va) {
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
            
            Object* instance = caller.object;
            if (!instance->active) {
                return sol::make_object(lua, sol::lua_nil);
            }
            sf::FloatRect otherRect = { { instance->bboxLeft(), instance->bboxTop() }, { 0, 0 } };
            otherRect.size.x = instance->bboxRight() - otherRect.position.x;
            otherRect.size.y = instance->bboxBottom() - otherRect.position.y;
            auto intersection = rect.findIntersection(otherRect);
            
            // Intersection vs none
            if (intersection.has_value()) {
                return sol::make_object(lua, instance->MyReference);
            }
            else {
                return sol::make_object(lua, sol::lua_nil);
            }
        }
        else {
            // Object
            auto& base = type.as<std::unique_ptr<BaseObject>&>();
            const Object* ignore = nullptr;
            if (va.size() > 0) {
                if (va[0].get<bool>() == true) {
                    ignore = caller.object;
                }
            }
            for (auto& i : ids) {
                auto& instance = i.second;
                if (i.second->active && i.second->extends(base.get())) {
                    if (ignore == nullptr || i.second != ignore) {
                        sf::FloatRect otherRect = { { instance->bboxLeft(), instance->bboxTop() }, { 0, 0 } };
                        otherRect.size.x = instance->bboxRight() - otherRect.position.x;
                        otherRect.size.y = instance->bboxBottom() - otherRect.position.y;
                        auto intersection = rect.findIntersection(otherRect);
                        if (intersection.has_value()) {
                            return sol::make_object(lua, i.second->MyReference);
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
        std::vector<Vector2f> callerPts = {
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
            if (!r.object->active) {
                return Object::Reference{ -1, sol::make_object(lua, sol::lua_nil) };
            }
            auto answer = polygonsIntersect(callerPts, r.object->getPoints());
            if (answer.intersect) {
                return r;
            }
        }

        auto& baseType = type.as<std::unique_ptr<BaseObject>&>();
        for (auto& i : ids) {
            if (i.second->active && i.second->extends(baseType.get())) {
                auto answer = polygonsIntersect(callerPts, i.second->getPoints());
                if (answer.intersect) {
                    return i.second->MyReference;
                }
            }
        }

        return Object::Reference { -1, sol::make_object(lua, sol::lua_nil) };
    }

    Object::Reference instanceCreateScript(float x, float y, float depth, BaseObject* baseObject) {
        Object* ptr = instanceCreate(x, y, depth, baseObject);

        auto it = ptr->kvp.find("create");
        if (it != ptr->kvp.end()) {
            auto res = it->second.as<sol::safe_function>()(ptr->MyReference, this);
            if (!res.valid()) {
                sol::error e = res;
                std::cout << e.what() << "\n";
            }
        }
        ptr->xPrev = ptr->x;
        ptr->yPrev = ptr->y;

        return ptr->MyReference;
    }

    Object* instanceCreate(float x, float y, float depth, BaseObject* baseObject) {
        std::unique_ptr<Object> copiedObject = ObjectManager::get().make(lua, baseObject);

        copiedObject->MyReference.id = currentId++;
        copiedObject->self = baseObject;
        copiedObject->MyReference = { copiedObject->MyReference.id, sol::make_object(lua, copiedObject.get()), copiedObject.get() };
        copiedObject->x = x;
        copiedObject->y = y;
        copiedObject->depth = depth;

        Object* ptr = copiedObject.get();

        addQueue.push_back(std::move(copiedObject));
        ids[ptr->MyReference.id] = ptr;

        if (ptr->kvp.find("draw") != ptr->kvp.end()) {
            ptr->drawFunc = ptr->kvp.find("draw")->second.as<sol::function>();
        }

        if (ptr->kvp.find("step") != ptr->kvp.end()) {
            ptr->stepFunc = ptr->kvp.find("step")->second.as<sol::function>();
        }

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