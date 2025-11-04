#include "sound.h"
#include "util/mathhelper.h"
#include "vendor/json.hpp"
#include <fstream>

using namespace nlohmann;

void SoundManager::initializeLua(sol::state &lua, const std::filesystem::path& assets) {
	auto paths = { assets / "sounds", assets / "music" };
	for (auto& p : paths) {
		for (auto& it : std::filesystem::directory_iterator(p)) {
			if (!it.is_regular_file()) continue;

			std::string soundName = it.path().filename().replace_extension("").string();
			if (lua[soundName] != sol::lua_nil) continue; // already contains sound

			std::filesystem::path soundFile = it.path();

			SoundAsset asset;
			asset.name = soundName;
			asset.path = soundFile;
			asset.volume = 1.0f;

			lua[soundName] = asset;
		}
	}

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

    lua["sound"]["set_loops"] = [&](sol::object sound, bool loops) {
		if (sound.is<SoundInstanceReference>()) {
			auto& inst = sound.as<SoundInstanceReference>();
			auto it = inst.buffer->instances.find(inst.id);
			if (it == inst.buffer->instances.end()) {
				return;
			}
			it->second->sound->setLooping(loops);
		}
	};

    lua["sound"]["is_playing"] = [&](sol::object sound) {
		if (sound.is<SoundAsset>()) {
			auto& asset = sound.as<SoundAsset>();
			auto it = buffers.find(asset.name);
			if (it == buffers.end()) {
				return false;
			}
			auto& bufs = it->second;
			if (!bufs.instances.empty()) {
				for (auto& i : bufs.instances) {
					if (i.second->sound->getStatus() == sf::Sound::Status::Playing && i.second->sound->getVolume() != 0 && i.second->sound->getPlayingOffset() < i.second->sound->getBuffer().getDuration()) {
						return true;
					}
				}
			}
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

    lua["sound"]["get_position"] = [&](sol::object sound) {
		if (!sound.is<SoundInstanceReference>()) {
			return 0.0f;
		}

		auto& inst = sound.as<SoundInstanceReference>();
		auto it = inst.buffer->instances.find(inst.id);
		if (it == inst.buffer->instances.end()) {
			return 0.0f;
		}

		float off = it->second->sound->getPlayingOffset().asSeconds();
		return off;
	};

	lua["sound"]["set_loops"] = [&](sol::object sound, bool loops) {
		if (!sound.is<SoundInstanceReference>()) {
			return;
		}

		auto& inst = sound.as<SoundInstanceReference>();
		auto it = inst.buffer->instances.find(inst.id);
		if (it == inst.buffer->instances.end()) {
			return;
		}

		it->second->sound->setLooping(loops);
	};

    lua["sound"]["set_position"] = [&](sol::object sound, float position) {
		if (!sound.is<SoundInstanceReference>()) {
			return;
		}

		auto& inst = sound.as<SoundInstanceReference>();
		auto it = inst.buffer->instances.find(inst.id);
		if (it == inst.buffer->instances.end()) {
			return;
		}

		it->second->sound->setPlayingOffset(sf::seconds(position));
	};

    lua["sound"]["stop"] = [&](sol::object sound) {
		if (sound.is<SoundAsset>()) {
			auto& asset = sound.as<SoundAsset>();
			auto& bufs = buffers[asset.name];
			for (auto& [k, i] : bufs.instances) {
				i->sound->setVolume(i->sound->getVolume() * 0.05f);
				std::lock_guard<std::mutex> lock(mutex);
				unloading.emplace_back(std::move(i->sound));
			}
			bufs.instances.clear();
		}
		else if (sound.is<SoundInstanceReference>()) {
			auto& ref = sound.as<SoundInstanceReference>();
			auto it = ref.buffer->instances.find(ref.id);
			if (it != ref.buffer->instances.end()) {
				it->second->sound->stop();
				// std::lock_guard<std::mutex> lock(mutex);
				// unloading.emplace_back(std::move(it->second->sound));
				ref.buffer->instances.erase(it);
			}
		}
    };
}

void SoundManager::update() {
	while (true) {
		int count = 0;
		{
			std::lock_guard<std::mutex> lock(mutex);
			count = unloading.size();
		}
		
		if (count > 0) {
			for (auto it = unloading.begin(); it != unloading.end(); it++) {
				std::lock_guard<std::mutex> lock(mutex);
				auto& snd = *it;
				snd->stop();
			}
			unloading.clear();
		}

		std::this_thread::sleep_for(std::chrono::nanoseconds(10));
	}
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