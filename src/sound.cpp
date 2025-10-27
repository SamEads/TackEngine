#include "sound.h"
#include "util/mathhelper.h"
#include "vendor/json.hpp"
#include <fstream>

using namespace nlohmann;

void SoundManager::initializeLua(sol::state &lua, const std::filesystem::path& assets) {
    for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "sounds")) {
        if (!it.is_directory()) continue;

        std::string soundName = it.path().filename().string();
		if (lua[soundName] != sol::lua_nil) continue; // already contains sound

        auto dataFile = it.path() / "data.json";
        std::ifstream i(dataFile);
        json j = json::parse(i);
        std::string extension = j["extension"];
        std::filesystem::path soundFile = it.path() / std::string("sound" + extension);

		SoundAsset asset;
		asset.name = soundName;
		asset.path = soundFile;
		asset.volume = j["volume"];

        lua[soundName] = asset;
    }

	lua.create_named_table("sound");

	lua["sound"]["play"] = [&](sol::object sound, float gain, float pitch) {
		if (!sound.is<SoundAsset>() && !sound.is<SoundInstanceReference>()) {
			return sol::make_object(lua, sol::lua_nil);
		}

		auto& asset = (sound.is<SoundAsset>()) ? sound.as<SoundAsset>() : *sound.as<SoundInstanceReference>().buffer->asset;
		
		if (buffers.find(asset.name) == buffers.end()) {
			auto& newbuf = buffers[asset.name];
			newbuf.asset = &asset;
			bool _ = newbuf.buffer.loadFromFile(asset.path);
		}

		auto& buf = buffers[asset.name];
		uint64_t id = buf.nextInstanceId;
		buf.nextInstanceId++;

		std::unique_ptr<SoundInstance> instance = std::make_unique<SoundInstance>();
		instance->buffer = &buf;
		instance->sound = std::make_unique<sf::Sound>(buf.buffer);
		instance->sound->setVolume((asset.volume * gain) * 100.0f);
		instance->sound->setPitch(pitch);
		instance->sound->play();

		auto instancePtr = instance.get();
		buf.instances[id] = std::move(instance);

		SoundInstanceReference ref;
		ref.buffer = &buf;
		ref.id = id;
		return sol::make_object(lua, ref);
    };

    lua["sound"]["is_playing"] = [&](sol::object sound) {
		if (sound.is<SoundAsset>()) {
			auto& asset = sound.as<SoundAsset>();
			auto& bufs = buffers[asset.name];
			return !bufs.instances.empty();
		}
		else if (sound.is<SoundInstanceReference>()) {
			auto& inst = sound.as<SoundInstanceReference>();
			auto it = inst.buffer->instances.find(inst.id);
			if (it == inst.buffer->instances.end()) {
				return false;
			}
			if (it->second->sound->getVolume() > 0.0f && it->second->sound->getStatus() == sf::Sound::Status::Playing) {
				return true;
			}
		}
		return false;
    };

    lua["sound"]["stop"] = [&](sol::object sound) {
		if (sound.is<SoundAsset>()) {
			auto& asset = sound.as<SoundAsset>();
			auto& bufs = buffers[asset.name];
			for (auto& [k, i] : bufs.instances) {
				i->sound->stop();
			}
			bufs.instances.clear();
		}
		else if (sound.is<SoundInstanceReference>()) {
			auto& ref = sound.as<SoundInstanceReference>();
			auto it = ref.buffer->instances.find(ref.id);
			if (it != ref.buffer->instances.end()) {
				it->second->sound->stop();
			}
		}
    };
}

void MusicManager::initializeLua(sol::state& lua, const std::filesystem::path& assets) {
	lua.create_named_table("music");

    lua["music"]["play"] = [&](SoundAsset& sound) {
		bool _ = music.openFromFile(sound.path);
		music.setLooping(true);
		music.play();
		music.setVolume(sound.volume * 100.0f);
		volume = 1.0f;
		asset = &sound;
    };
    lua["music"]["set_loop_points"] = [&](float a, float b) {
		if (a == 0 && b == 0) return;

		sf::Music::TimeSpan timespan;
		timespan.offset = sf::seconds(a);
		timespan.length = sf::seconds(b) - timespan.offset;
		music.setLoopPoints(timespan);
    };
    lua["music"]["stop"] = [&]() {
		music.stop();
    };
    lua["music"]["pause"] = [&]() {
		music.pause();
    };
    lua["music"]["get_position"] = [&]() {
        return music.getPlayingOffset().asSeconds();
    };
	lua["music"]["set_loops"] = [&](bool loops) {
		music.setLooping(loops);
    };
	lua["music"]["is_playing"] = [&]() {
        return music.getStatus() == sf::Music::Status::Playing;
    };
    lua["music"]["set_position"] = [&](float position) {
		music.setPlayingOffset(sf::seconds(position));
    };
    lua["music"]["set_volume"] = [&](float volume) {
		music.setVolume(this->volume * volume * 100.0f);
    };
    lua["music"]["set_pitch"] = [&](float pitch) {
		music.setPitch(pitch);
    };
}