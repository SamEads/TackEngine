#pragma once

#include <string>
#include <unordered_map>
#include <SFML/Audio.hpp>
#include <thread>
#include <mutex>

class ScriptSound {
public:
	std::filesystem::path file;
	float volume = 1.0f;
};

class SoundManager {
public:
	class SoundData {
	public:
		sf::Clock timer;
		std::unique_ptr<sf::Sound> sound;
	};
	class Sounds {
	public:
		sf::SoundBuffer buffer;
		std::vector<SoundData> sounds;
	};
	std::unordered_map<std::string, Sounds> sounds;
	std::thread thread;
	std::mutex mutex;
	bool running = true;

	class FadeOutSound {
	public:
		std::unique_ptr<sf::Sound> sound;
		float startValue;
		float seconds;
		sf::Clock timer;
	};
	std::vector<FadeOutSound> fadeOutSounds;
	void play(std::string sound, float pitch = 1.0f, float volume = 1.0f, bool loop = false);
	void stop(std::string sound);
	void fadeOut(std::string sound, float seconds);
	void update();
	void shutdown();
	bool isPlaying(std::string sound);

    static SoundManager& get() {
        static SoundManager sndMgr;
        return sndMgr;
    }
};