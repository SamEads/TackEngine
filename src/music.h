#include "sound.h"

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