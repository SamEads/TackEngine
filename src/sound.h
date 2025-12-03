#pragma once

#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <SFML/Audio.hpp>
#include "luainc.h"

class SoundAsset {
public:
	std::string name;
	std::filesystem::path path;
	float volume;
};

class SoundBuffer;
class SoundInstance {
public:
	SoundBuffer* buffer;
	std::unique_ptr<sf::Sound> sound;
};

class SoundBuffer {
public:
	sf::SoundBuffer buffer;
	SoundAsset* asset;
	std::unordered_map<uint64_t, std::unique_ptr<SoundInstance>> instances;
	uint64_t nextInstanceId;
};

class SoundManager {
public:
	std::thread thread;
	std::mutex mutex;
	std::unordered_map<std::string, SoundBuffer> buffers;
	std::vector<std::unique_ptr<sf::Sound>> unloading;
	void initializeLua(LuaState& L, const std::filesystem::path& assets);
	void update();
    static SoundManager& get() {
        static SoundManager sndMgr;
        return sndMgr;
    }
};

class MusicManager {
public:
	bool paused = false;
	float volume;
	sf::Music music;
	SoundAsset* asset;
	void initializeLua(LuaState& L, const std::filesystem::path& assets);
	static MusicManager& get() {
        static MusicManager musMgr;
        return musMgr;
    }
};