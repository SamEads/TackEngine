#pragma once

#include <iostream>
#include "object.h"
#include "collision.h"
#include "tilemap.h"
#include "roomreference.h"

class Background : public Object {
public:
    std::string name;
    bool tiledX, tiledY;
    bool offsetX, offsetY;
    sf::Color color = { 255, 255, 255, 255 };

    Background(sol::state& lua) : Object(lua) {}
    void draw(Room* room, float alpha) override;
};

class Room {
private:
    class Camera {
    public:
        bool stayInBounds = true;
        Room* room;
        float x = 0, y = 0;
        float xPrev = 0, yPrev = 0;
        float width, height;
        float getX() const { return x; }
        float getY() const { return y; }
        void setX(float x) {
            if (!stayInBounds) {
                this->x = x;
            }
            else {
                this->x = std::clamp(x, 0.0f, room->width - width);
            }
        }
        void setY(float y) {
            if (!stayInBounds) {
                this->y = y;
            }
            else {
                this->y = std::clamp(y, 0.0f, room->height - height);
            }
        }
        float getXPrev() const { return xPrev; }
        float getYPrev() const { return yPrev; }
        float getWidth() const { return width; }
        float getHeight() const { return height; }
        void teleport(float x, float y) {
            setX(x);
            setY(y);
            this->xPrev = this->x;
            this->yPrev = this->y;
        }
    };
    const RoomReference* roomReference;
    std::vector<Object*> drawables;
    void createAndRoomStartEvents();
public:
    static void initializeLua(sol::state& lua, const std::filesystem::path& assets);

    ObjectId myId;
    ObjectId currentId = 0;
    
    std::vector<std::unique_ptr<Object>> instances;
    std::vector<Background*> backgrounds;
    std::vector<Tilemap*> tilemaps;

    std::vector<std::unique_ptr<Object>> addQueue;
    std::vector<size_t> deleteQueue;
    
    std::unordered_map<ObjectId, Object*> ids;
    sol::state& lua;

    int minReserved = 0;
    int width, height;
    Camera camera;
    float renderCameraX = 0, renderCameraY = 0;

    Room(sol::state& lua, RoomReference* data);
    Room(sol::state& lua);
    ~Room() = default;

    void load();

    void timestep();

    void draw(float alpha);

    void setView(float cx, float cy);

    void deactivateObject(sol::object object);
    void activateObject(sol::object object);
    void activateObjectRegion(sol::object object, float x1, float y1, float x2, float y2);

    void updateQueue() {
        // Add queued objects
        int size = instances.size();
        for (auto& o : addQueue) {
            o->vectorPos = size;
            instances.push_back(std::move(o));
            size++;
        }

        // Delete objects queued for deletion
        if (!deleteQueue.empty()) {
            std::sort(deleteQueue.begin(), deleteQueue.end(), std::greater<size_t>());
            for (auto& pos : deleteQueue) {
                size_t size = instances.size();
                if (pos >= size - 1) {
                    instances.pop_back();
                }
                else {
                    std::swap(instances[pos], instances[instances.size() - 1]);
                    instances.pop_back();
                    instances[pos]->vectorPos = pos;
                }
            }
            deleteQueue.clear();
        }

        addQueue.clear();
        deleteQueue.clear();
    }

    void objectDestroy(std::unique_ptr<BaseObject>& base) {
        for (auto& i : instances) {
            if (i->extends(base.get())) {
                ObjectId id = i->MyReference.id;
                auto idPos = ids.find(id);
                if (idPos != ids.end()) {
                    deleteQueue.push_back(i->vectorPos);
                    ids.erase(id);
                }
            }
        }
    }

    void instanceDestroy(ObjectId id) {
        auto idPos = ids.find(id);
        if (idPos != ids.end()) {
            deleteQueue.push_back(id);
            ids.erase(id);
        }
    }

    void instanceDestroyScript(sol::object obj) {
        if (obj.is<Object::Reference>()) {
            Object::Reference& ref = obj.as<Object::Reference>();
            Object* object = ref.object;

            auto idPos = ids.find(object->MyReference.id);
            if (idPos != ids.end()) {
                object->runScript("destroy", this);
                deleteQueue.push_back(object->vectorPos);
                ids.erase(object->MyReference.id);
            }
        }
        else if (obj.is<BaseObject*>()) {
            std::unique_ptr<BaseObject>& base = obj.as<std::unique_ptr<BaseObject>&>();
            for (auto& i : instances) {
                if (i->extends(base.get())) {
                    ObjectId id = i->MyReference.id;
                    auto idPos = ids.find(id);
                    if (idPos != ids.end()) {
                        deleteQueue.push_back(i->vectorPos);
                        ids.erase(id);
                    }
                }
            }
            return;
        }
        return;
    }

    int objectCount(BaseObject* baseType) {
        if (baseType == NULL) {
            return false;
        }
        int c = 0;
        for (auto& i : instances) {
            if (i->extends(baseType)) {
                c++;
            }
        }
        return c;
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
            ObjectId refId = ref->id;
            return ids.find(refId) != ids.end();
        }
        else if (obj.is<BaseObject>()) {
            std::unique_ptr<BaseObject>& ref = obj.as<std::unique_ptr<BaseObject>&>();
            BaseObject* refptr = ref.get();
            for (auto& i : ids) {
                if (i.second->extends(refptr)) {
                    return true;
                }
            }
            return false;
        }
        else {
            return false;
        }
    }

    bool instanceExists(Object::Reference reference) {
        int refId = reference.id;
        if (reference.roomId != myId) {
            return false;
        }
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
        Precise
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
            return Object::Reference { -1, this->myId, sol::make_object(lua, sol::lua_nil) };
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
                return Object::Reference { -1, this->myId, sol::make_object(lua, sol::lua_nil) };
            }
            if (!r.object->active) {
                return Object::Reference { -1, this->myId, sol::make_object(lua, sol::lua_nil) };
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

        return Object::Reference { -1, 0, sol::make_object(lua, sol::lua_nil) };
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
        ptr->xPrevRender = ptr->xPrev = ptr->x;
        ptr->yPrevRender = ptr->yPrev = ptr->y;

        return ptr->MyReference;
    }

    Object* instanceCreate(float x, float y, float depth, BaseObject* baseObject) {
        std::unique_ptr<Object> copiedObject = ObjectManager::get().makeInstance(lua, baseObject);
        
        copiedObject->MyReference.id = currentId++;
        copiedObject->self = baseObject;
        copiedObject->MyReference = Object::Reference {
            copiedObject->MyReference.id,
            this->myId,
            sol::make_object(lua, copiedObject.get()),
            copiedObject.get()
        };

        copiedObject->x = x;
        copiedObject->y = y;
        copiedObject->depth = depth;

        Object* ptr = copiedObject.get();

        addQueue.push_back(std::move(copiedObject));
        ids[ptr->MyReference.id] = ptr;

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