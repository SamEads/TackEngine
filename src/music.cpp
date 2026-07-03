#include "music.h"

static int MusicPlay(lua_State* L) {
	if (auto sound = lua_testclass<SoundAsset>(L, 1, "SoundAsset")) {
		auto& mm = MusicManager::get();
		if (mm.music.openFromFile(sound->path)) {}
		mm.music.setLooping(true);
		mm.music.play();
		mm.music.setVolume(sound->volume * 100.0f);
		mm.volume = 1.0f;
		mm.asset = sound;
	}
	return 0;
}

static int MusicStop(lua_State* L) {
	MusicManager::get().music.stop();
	return 0;
}

static int MusicSetLoopPoints(lua_State* L) {
	float a = luaL_checknumber(L, 1);
	float b = luaL_checknumber(L, 2);
	if (a == 0 && b == 0) {
		return 0;
	}

	MusicManager& mm = MusicManager::get();
	sf::Music::TimeSpan timespan;
	timespan.offset = sf::seconds(a);
	timespan.length = sf::seconds(b) - timespan.offset;
	mm.music.setLoopPoints(timespan);

	return 0;
}

static int MusicUnpause(lua_State* L) {
	MusicManager& mm = MusicManager::get();
	if (mm.music.getStatus() == sf::Music::Status::Paused) {
		mm.music.play();
	}
	return 0;
}

static int MusicSetLoops(lua_State* L) {
	MusicManager& mm = MusicManager::get();
	bool doLoop = lua_toboolean(L, 1);
	mm.music.setLooping(doLoop);
	return 0;
}

static int MusicPause(lua_State* L) {
	MusicManager::get().music.pause();
	return 0;
}

static int MusicIsPlaying(lua_State* L) {
	MusicManager& mm = MusicManager::get();
	lua_pushboolean(L, mm.music.getStatus() == sf::Music::Status::Playing);
	return 1;
}

static int MusicGetPosition(lua_State* L) {
	MusicManager& mm = MusicManager::get();
	lua_pushnumber(L, mm.music.getPlayingOffset().asSeconds());
	return 1;
}

static int MusicSetPosition(lua_State* L) {
	float position = luaL_checknumber(L, 1);
	MusicManager& mm = MusicManager::get();
	mm.music.setPlayingOffset(sf::seconds(position));
	return 0;
}

static int MusicSetPitch(lua_State* L) {
	float pitch = lua_tonumber(L, 1);
	MusicManager& mm = MusicManager::get();
	mm.music.setPitch(pitch);
	return 0;
}

void MusicManager::initializeLua(LuaState& L, const std::filesystem::path& assets) {
	lua_getglobal(L, ENGINE_ENV);
		lua_newtable(L); // Music
			lua_pushcfunction(L, MusicPlay);
			lua_setfield(L, -2, "play");

			lua_pushcfunction(L, MusicStop);
			lua_setfield(L, -2, "stop");

			lua_pushcfunction(L, MusicSetLoopPoints);
			lua_setfield(L, -2, "set_loop_points");

			lua_pushcfunction(L, MusicUnpause);
			lua_setfield(L, -2, "unpause");

			lua_pushcfunction(L, MusicSetLoops);
			lua_setfield(L, -2, "set_loops");

			lua_pushcfunction(L, MusicPause);
			lua_setfield(L, -2, "pause");

			lua_pushcfunction(L, MusicIsPlaying);
			lua_setfield(L, -2, "is_playing");

			lua_pushcfunction(L, MusicGetPosition);
			lua_setfield(L, -2, "get_position");

			lua_pushcfunction(L, MusicSetPosition);
			lua_setfield(L, -2, "set_position");

			lua_pushcfunction(L, MusicSetPosition);
			lua_setfield(L, -2, "set_volume");

			lua_pushcfunction(L, MusicSetPitch);
			lua_setfield(L, -2, "set_pitch");
		lua_setfield(L, -2, "music");
	lua_pop(L, 1);
}