#pragma once

#include <string>
#include <unordered_map>
#include <SFML/Audio.hpp>
#include <thread>
#include <mutex>
#include <sol/sol.hpp>

class ScriptSound {
public:
	std::filesystem::path file;
	float volume = 1.0f;
};

class SoundData {
public:
	sf::Clock timer;
	std::unique_ptr<sf::Sound> sound;
	uint64_t index;
};

class Sounds {
public:
	const ScriptSound* data;
	bool deferLoad = false;
	bool playOnLoad = false;
	struct ToPlay {
		float pitch, volume;
		bool loop;
	} toPlay;
	uint64_t soundIndex;
	
	sf::SoundBuffer buffer;
	std::vector<SoundData> sounds;
	std::unordered_map<uint64_t, SoundData*> soundIndexes;
};

class SoundReference {
public:
	bool valid = false;
	Sounds* sounds;
	uint64_t index;
};

class SoundManager {
public:
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
	SoundReference play(const ScriptSound& sound, float pitch = 1.0f, float volume = 1.0f, bool loop = false);
	void stop(sol::object object);
	void fadeOut(const ScriptSound& sound, float seconds);
	void update();
	void shutdown();
	bool isPlaying(const ScriptSound& sound);
	void initializeLua(sol::state& lua, const std::filesystem::path& assets);

    static SoundManager& get() {
        static SoundManager sndMgr;
        return sndMgr;
    }
};

class MusicManager {
public:
	bool hasMusic = false;
	sf::Music m;
	ScriptSound* currentSound;
	float currentVolume = 1.0f;
	void play(ScriptSound& sound) {
		bool result = m.openFromFile(sound.file);
		m.setLooping(true);
		m.play();
		m.setVolume(sound.volume * 100.0f);
		currentSound = &sound;
		currentVolume = 1.0f;
		hasMusic = true;
	}
	void stop() {
		if (!hasMusic) return;
		m.stop();
	}
	void pause() {
		if (!hasMusic) return;
		m.pause();
	}
	void setPitch(float pitch) {
		m.setPitch(pitch);
	}
	void resume() {
		if (!hasMusic) return;
		m.play();
	}
	void setVolume(float volume) {
		if (!hasMusic) return;
		currentVolume = volume;
		m.setVolume((currentSound->volume * currentVolume) * 100.0f);
	}
	void setLoopPoints(float start, float end) {
		if (!hasMusic) return;
		sf::Music::TimeSpan timespan;
		timespan.offset = sf::seconds(start);
		timespan.length = sf::seconds(end) - timespan.offset;
		m.setLoopPoints(timespan);
	}
	float getPosition() {
		if (!hasMusic) return 0;
		return m.getPlayingOffset().asSeconds();
	}
	void setPosition(float position) {
		if (!hasMusic) return;
		m.setPlayingOffset(sf::seconds(position));
	}
	static MusicManager& get() {
        static MusicManager musMgr;
        return musMgr;
    }
	void initializeLua(sol::state& lua, const std::filesystem::path& assets);
};