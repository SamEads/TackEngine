#include "sound.h"
#include "mathhelper.h"
#include "vendor/json.hpp"
#include <fstream>

using namespace nlohmann;

void SoundManager::update() {
	while (true) {
		{
			std::lock_guard<std::mutex> s(mutex);
			if (!running) {
				break;
			}
		}

		{
			std::lock_guard<std::mutex> s(mutex);
			for (auto it = fadeOutSounds.begin(); it != fadeOutSounds.end();) {
				if ((*it).sound->getStatus() != sf::Sound::Status::Playing || ((*it).sound->getVolume() <= 0.0f)) {
					it = fadeOutSounds.erase(it);
				}
				else {
					++it;
				}
			}
			for (auto& [k, v] : sounds) {
				auto& vec = v.sounds;
				for (auto it = vec.begin(); it != vec.end();) {
					std::unique_ptr<sf::Sound>& s = it->sound;
					auto soundStatus = s->getStatus();
					float volume = s->getVolume();
					if (soundStatus == sf::Sound::Status::Stopped || volume <= 0.0f) {
						if (volume <= 0.0f || it->timer.getElapsedTime().asSeconds() > v.buffer.getDuration().asSeconds() + 1.0f) {
							it = vec.erase(it);
							continue;
						}
					}
					++it;
				}
			}
		}

		{
			std::lock_guard<std::mutex> s(mutex);
			for (auto& fadeoutsound : fadeOutSounds) {
				auto& sound = fadeoutsound.sound;
				auto vol = sound->getVolume();

				auto& timer = fadeoutsound.timer;
				float progress = std::clamp(timer.getElapsedTime().asSeconds() / fadeoutsound.seconds, 0.0f, 1.0f);
				float value = lerp(fadeoutsound.startValue, 0.0f, progress);

				sound->setVolume(value);
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

void SoundManager::shutdown() {
	{
		std::lock_guard<std::mutex> s(mutex);
		running = false;
	}
	if (thread.joinable()) {
		thread.join();
	}
}

bool SoundManager::isPlaying(std::string sound) {
	std::lock_guard<std::mutex> s(mutex);

	auto it = sounds.find(sound);
	if (sounds.find(sound) != sounds.end()) {
		auto& vec = (*it).second.sounds;
		for (auto& sound : vec) {
			float volume = sound.sound->getVolume();
			bool status = sound.sound->getStatus() == sf::Sound::Status::Playing;
			if (status && volume > 0) {
				return true;
			}
		}
	}

	return false;
}

void SoundManager::initializeLua(sol::state &lua, const std::filesystem::path& assets) {
    lua.new_usertype<ScriptSound>(
        "Sound", sol::no_constructor
    );

    // Sounds
    for (auto& it : std::filesystem::directory_iterator(assets / "sounds")) {
        if (!it.is_directory()) continue;

        std::string soundName = it.path().filename().string();

        auto data = it.path() / "data.json";
        std::ifstream i(data);
        json j = json::parse(i);
        std::string extension = j["extension"].get<std::string>();

        ScriptSound scriptSound;
        scriptSound.file = std::filesystem::path(soundName) / std::string("sound" + extension);
        scriptSound.volume = j["volume"];

        lua[soundName] = scriptSound;
    }

	lua.create_named_table("sound");
    lua["sound"]["is_playing"] = [&](const ScriptSound& sound) {
        return isPlaying(sound.file.string());
    };
    lua["sound"]["play"] = [&](const ScriptSound& sound, float gain, float pitch) {
        play(sound.file.string(), pitch, sound.volume * gain);
    };
    lua["sound"]["stop"] = [&](const ScriptSound& sound) {
        stop(sound.file.string());
    };
}

void SoundManager::play(std::string sound, float pitch, float volume, bool loop) {
	std::lock_guard<std::mutex> s(mutex);

	if (sounds.find(sound) == sounds.end()) {
		bool loadedSound = sounds[sound].buffer.loadFromFile(std::filesystem::path("assets") / "sounds" / sound);
	}

	auto& buf = sounds[sound].buffer;
	sounds[sound].sounds.emplace_back(SoundData { sf::Clock(), std::make_unique<sf::Sound>(buf) });
	sounds[sound].sounds.back().timer.restart();
	sounds[sound].sounds.back().sound->play();
	sounds[sound].sounds.back().sound->setPitch(pitch);
	sounds[sound].sounds.back().sound->setVolume(volume * 100.0f);
	sounds[sound].sounds.back().sound->setLooping(loop);
}

void SoundManager::stop(std::string sound) {
	std::lock_guard<std::mutex> s(mutex);

	auto it = sounds.find(sound);
	if (it != sounds.end()) {
		auto& vec = (*it).second.sounds;
		for (auto& sound : vec) {
			float volume = sound.sound->getVolume();
			fadeOutSounds.push_back(FadeOutSound { std::move(sound.sound), volume, 0.05f });
			fadeOutSounds.back().timer.restart();
		}
		vec.clear();
	}
}

void SoundManager::fadeOut(std::string sound, float seconds) {
	std::lock_guard<std::mutex> s(mutex);

	auto it = sounds.find(sound);
	if (it != sounds.end()) {
		auto& vec = (*it).second.sounds;
		for (auto& sound : vec) {
			float startVolume = sound.sound->getVolume();
			fadeOutSounds.push_back(FadeOutSound { std::move(sound.sound), startVolume, seconds });
			fadeOutSounds.back().timer.restart();
		}
		vec.clear();
	}
}

void MusicManager::initializeLua(sol::state& lua) {
	lua.create_named_table("music");
    lua["music"]["play"] = [&](ScriptSound& sound) {
		play(sound);
    };
    lua["music"]["set_loop_points"] = [&](float a, float b) {
        setLoopPoints(a, b);
    };
    lua["music"]["stop"] = [&]() {
        stop();
    };
    lua["music"]["pause"] = [&]() {
        pause();
    };
    lua["music"]["get_position"] = [&]() {
        return getPosition();
    };
    lua["music"]["set_position"] = [&](float position) {
        setPosition(position);
    };
    lua["music"]["set_volume"] = [&](float volume) {
        setVolume(volume);
    };
    lua["music"]["set_pitch"] = [&](float pitch) {
        setPitch(pitch);
    };
}