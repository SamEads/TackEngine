#pragma once

#include <string>
#include <unordered_map>
#include <SFML/Audio.hpp>
#include <thread>
#include <mutex>
#include <sol/sol.hpp>

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

class SoundInstanceReference {
public:
	uint64_t id;
	SoundBuffer* buffer;
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
	std::thread loadThread;
	std::mutex mutex;
	std::unordered_map<std::string, SoundBuffer> buffers;
	void initializeLua(sol::state& lua, const std::filesystem::path& assets);
    static SoundManager& get() {
        static SoundManager sndMgr;
        return sndMgr;
    }
};

class MusicManager {
public:
	float volume;
	sf::Music music;
	SoundAsset* asset;
	void initializeLua(sol::state& lua, const std::filesystem::path& assets);
	static MusicManager& get() {
        static MusicManager musMgr;
        return musMgr;
    }
};