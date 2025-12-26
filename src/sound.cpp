#include "sound.h"
#include "util/mathhelper.h"
#include "vendor/json.hpp"
#include <fstream>

using namespace nlohmann;

void SoundManager::initializeLua(LuaState& L, const std::filesystem::path& assets) {
	luaL_newmetatable(L, "SoundAsset");
	lua_pop(L, 1);

	luaL_newmetatable(L, "SoundInstance");
	lua_pop(L, 1);

	lua_getglobal(L, ENGINE_ENV);
		auto paths = {
			assets / "sounds",
			assets / "music"
		};
		for (auto& p : paths) {
			for (auto& it : std::filesystem::directory_iterator(p)) {
				if (!it.is_regular_file()) continue;

				std::string soundName = it.path().filename().replace_extension("").string();

				lua_getfield(L, -1, soundName.c_str()); // snd,soundmodule,te
					bool exists = !lua_isnil(L, -1);
				lua_pop(L, 1); // soundmodule,te

				if (exists) continue;

				lua_newtable(L);
					std::filesystem::path soundFile = it.path();
					SoundAsset* soundAsset = new (lua_newuserdata(L, sizeof(SoundAsset)) ) SoundAsset(); // userdata,soundmodule,te
						soundAsset->name = soundName;
						soundAsset->path = soundFile;
						soundAsset->volume = 1.0f;
					lua_setfield(L, -2, "__cpp_ptr");
					luaL_setmetatable(L, "SoundAsset");
				lua_setfield(L, -2, soundName.c_str()); // soundmodule,te
			}
		}

		for (auto& it : std::filesystem::directory_iterator(assets / "managed" / "sounds")) {
			if (!it.is_directory()) continue;

			std::string soundName = it.path().filename().string();

			lua_getfield(L, -1, soundName.c_str());
				bool exists = !lua_isnil(L, -1);
			lua_pop(L, 1);

			if (exists) {
				continue;
			}

			auto dataFile = it.path() / "data.json";
			std::ifstream i(dataFile);
			json j = json::parse(i);
			std::string extension = j["extension"];
			std::filesystem::path soundFile = it.path() / std::string("sound" + extension);

			lua_newtable(L);
				SoundAsset* soundAsset = new ( lua_newuserdata(L, sizeof(SoundAsset)) ) SoundAsset(); // userdata,soundmodule,te
					soundAsset->name = soundName;
					soundAsset->path = soundFile;
					soundAsset->volume = 1.0f;
				lua_setfield(L, -2, "__cpp_ptr");
				luaL_setmetatable(L, "SoundAsset");
			lua_setfield(L, -2, soundName.c_str()); // soundmodule,te
		}

		lua_newtable(L); // sound,te

			// Sound, gain, pitch
			lua_pushcfunction(L, [](lua_State* L) -> int {
				SoundManager& sm = SoundManager::get();
				SoundAsset* asset = lua_testclass<SoundAsset>(L, 1, "SoundAsset");
				float gain = luaL_checknumber(L, 2);
				float pitch = luaL_checknumber(L, 3);

				if (asset == nullptr) {
					SoundInstance* instance = lua_testclass<SoundInstance>(L, 1, "SoundInstance");
					if (!instance) {
						lua_pushnil(L);
						return 1;
					}
					asset = instance->buffer->asset;
				}

				if (sm.buffers.find(asset->name) == sm.buffers.end()) {
					auto& newbuf = sm.buffers[asset->name];
					newbuf.asset = asset;
					bool _ = newbuf.buffer.loadFromFile(asset->path);
				}

				auto& buf = sm.buffers[asset->name];
				uint64_t id = buf.nextInstanceId++;

				std::unique_ptr<SoundInstance> instance = std::make_unique<SoundInstance>();
				instance->buffer = &buf;
				instance->sound = std::make_unique<sf::Sound>(buf.buffer);
				instance->sound->setVolume((asset->volume * gain) * 100.0f);
				instance->sound->setPitch(pitch);
				instance->sound->play();

				auto instancePtr = instance.get();
				buf.instances[id] = std::move(instance);

				lua_newtable(L);
					lua_pushlightuserdata(L, &buf);
					lua_setfield(L, -2, "__buffer");
					lua_pushinteger(L, id);
					lua_setfield(L, -2, "__id");
					lua_pushlightuserdata(L, instancePtr);
					lua_setfield(L, -2, "__cpp_ptr");
					luaL_setmetatable(L, "SoundInstance");
				return 1;
			});
			lua_setfield(L, -2, "play");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				if (auto inst = lua_testclass<SoundInstance>(L, 1, "SoundInstance")) {
					inst->sound->setLooping(lua_toboolean(L, 1));
				}
				return 0;
			});
			lua_setfield(L, -2, "set_loops");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				if (lua_isnil(L, 1)) {
					lua_pushboolean(L, false);
					return 1;
				}

				SoundManager& sm = SoundManager::get();

				if (auto asset = lua_testclass<SoundAsset>(L, 1, "SoundAsset")) {
					auto it = sm.buffers.find(asset->name);
					if (it == sm.buffers.end()) {
						lua_pushboolean(L, false);
						return 1;
					}
					auto& bufs = it->second;
					if (!bufs.instances.empty()) {
						for (auto& i : bufs.instances) {
							if (i.second->sound->getStatus() == sf::Sound::Status::Playing && i.second->sound->getVolume() != 0 && i.second->sound->getPlayingOffset() < i.second->sound->getBuffer().getDuration()) {
								lua_pushboolean(L, true);
								return 1;
							}
						}
					}

					lua_pushboolean(L, false);
					return 1;
				}
				
				if (auto instance = lua_testclass<SoundInstance>(L, 1, "SoundInstance")) {
					lua_getfield(L, 1, "__id");
					int id = lua_tointeger(L, -1);
					lua_pop(L, 1);

					lua_getfield(L, 1, "__buffer");
					SoundBuffer* buf = static_cast<SoundBuffer*>(lua_touserdata(L, -1));
					lua_pop(L, 1);

					auto it = buf->instances.find(id);

					if (it == buf->instances.end()) {
						lua_pushboolean(L, false);
						return 1;
					}

					if (it->second->sound->getVolume() > 0.0f && it->second->sound->getStatus() == sf::Sound::Status::Playing) {
						lua_pushboolean(L, true);
						return 1;
					}

					lua_pushboolean(L, false);
					return 1;
				}

				lua_pushboolean(L, false);
				return 1;
			});
			lua_setfield(L, -2, "is_playing");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				if (!lua_isclass<SoundInstance>(L, 1, "SoundInstance")) {
					lua_pushnumber(L, 0.0);
					return 1;
				}

				lua_getfield(L, 1, "__buffer");
				if (lua_isnil(L, -1)) {
					lua_pop(L, 1);
					lua_pushnumber(L, 0.0);
					return 1;
				}
				SoundBuffer* buffer = static_cast<SoundBuffer*>(lua_touserdata(L, -1));
				lua_pop(L, 1);

				lua_getfield(L, 1, "__id");
				if (lua_isnil(L, -1)) {
					lua_pop(L, 1);
					lua_pushnumber(L, 0.0);
					return 1;
				}
				int id = lua_tointeger(L, -1);
				lua_pop(L, 1);

				auto it = buffer->instances.find(id);
				if (it == buffer->instances.end()) {
					lua_pushnumber(L, 0.0);
					return 1;
				}

				lua_pushnumber(L, it->second->sound->getPlayingOffset().asSeconds());
				return 1;
			});
			lua_setfield(L, -2, "get_position");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				if (!lua_isclass<SoundInstance>(L, 1, "SoundInstance")) {
					lua_pushnumber(L, 0.0);
					return 1;
				}

				lua_getfield(L, 1, "__buffer");
				if (lua_isnil(L, -1)) {
					lua_pop(L, 1);
					return 0;
				}
				SoundBuffer* buffer = static_cast<SoundBuffer*>(lua_touserdata(L, -1));
				lua_pop(L, 1);

				lua_getfield(L, 1, "__id");
				if (lua_isnil(L, -1)) {
					lua_pop(L, 1);
					return 0;
				}
				int id = lua_tointeger(L, -1);
				lua_pop(L, 1);

				auto it = buffer->instances.find(id);
				if (it == buffer->instances.end()) {
					return 0;
				}

				it->second->sound->setPlayingOffset(sf::seconds(lua_tonumber(L, 2)));
				return 0;
			});
			lua_setfield(L, -2, "set_position");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				SoundManager& sm = SoundManager::get();
				for (auto& it : sm.buffers) {
					for (auto& it2 : it.second.instances) {
						it2.second->sound->stop();
					}
				}
				return 0;
			});
			lua_setfield(L, -2, "stop_all");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				if (lua_isnil(L, 1)) {
					return 0;
				}

				SoundManager& sm = SoundManager::get();
				SoundAsset* asset = lua_testclass<SoundAsset>(L, 1, "SoundAsset");

				if (SoundAsset* asset = lua_testclass<SoundAsset>(L, 1, "SoundAsset")) {
					auto buf = sm.buffers.find(asset->name);
					if (buf != sm.buffers.end()) {
						for (auto& [k, i] : buf->second.instances) {
							i->sound->stop();
						}
						buf->second.instances.clear();
					}
					return 0;
				}

				if (lua_isclass<SoundInstance>(L, 1, "SoundInstance")) {
					lua_getfield(L, 1, "__buffer");
					if (!lua_isnil(L, -1)) {
						SoundBuffer* buffer = static_cast<SoundBuffer*>(lua_touserdata(L, -1));
						lua_pop(L, 1);
						lua_getfield(L, 1, "__id");
						int id = lua_tointeger(L, -1);
						lua_pop(L, 1);
						auto it = buffer->instances.find(id);
						if (it != buffer->instances.end()) {
							it->second->sound->stop();
							buffer->instances.erase(it);
						}
					}
					else {
						lua_pop(L, 1);
					}
					return 0;
				}

				return 0;
			});
			lua_setfield(L, -2, "stop");

		lua_setfield(L, -2, "sound"); // te
	lua_pop(L, 1);
}

void SoundManager::update() {
	while (true) {
		int count = 0;
		{
			std::lock_guard<std::mutex> lock(mutex);
			count = unloading.size();
		}

		{
			std::lock_guard<std::mutex> lock(mutex);
			for (auto& [k, v] : buffers) {
				for (auto it = v.instances.begin(); it != v.instances.end();) {
					if (it->second->sound->getPlayingOffset() >= v.buffer.getDuration() ||
						it->second->sound->getStatus() == sf::Sound::Status::Stopped) {
						it = v.instances.erase(it);
						continue;
					}
					++it;
				}
			}
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

void MusicManager::initializeLua(LuaState& L, const std::filesystem::path& assets) {
	lua_getglobal(L, ENGINE_ENV);
		lua_newtable(L); // Music
			lua_pushcfunction(L, [](lua_State* L) -> int {
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
			});
			lua_setfield(L, -2, "play");

			lua_pushcfunction(L, [](lua_State* L) -> int { MusicManager::get().music.stop(); return 0; });
			lua_setfield(L, -2, "stop");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				float a = luaL_checknumber(L, 1);
				float b = luaL_checknumber(L, 2);
				if (a == 0 && b == 0) {
					return 0;
				}

				auto& mm = MusicManager::get();
				sf::Music::TimeSpan timespan;
				timespan.offset = sf::seconds(a);
				timespan.length = sf::seconds(b) - timespan.offset;
				mm.music.setLoopPoints(timespan);

				return 0;
			});
			lua_setfield(L, -2, "set_loop_points");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				auto& mm = MusicManager::get();
				if (mm.music.getStatus() == sf::Music::Status::Paused) {
					mm.music.play();
				}
				return 0;
			});
			lua_setfield(L, -2, "unpause");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				auto& mm = MusicManager::get();
				bool doLoop = lua_toboolean(L, 1);
				mm.music.setLooping(doLoop);
				return 0;
			});
			lua_setfield(L, -2, "set_loops");

			lua_pushcfunction(L, [](lua_State* L) -> int { MusicManager::get().music.pause(); return 0; });
			lua_setfield(L, -2, "pause");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				auto& mm = MusicManager::get();
				lua_pushboolean(L, mm.music.getStatus() == sf::Music::Status::Playing);
				return 1;
			});
			lua_setfield(L, -2, "is_playing");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				auto& mm = MusicManager::get();
				lua_pushnumber(L, mm.music.getPlayingOffset().asSeconds());
				return 1;
			});
			lua_setfield(L, -2, "get_position");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				float position = luaL_checknumber(L, 1);
				auto& mm = MusicManager::get();
				mm.music.setPlayingOffset(sf::seconds(position));
				return 0;
			});
			lua_setfield(L, -2, "set_position");

			lua_pushcfunction(L, [](lua_State* L) -> int {
				float volume = luaL_checknumber(L, 1);
				auto& mm = MusicManager::get();
				mm.music.setVolume(mm.volume * volume * 100.0f);
				return 0;
			});
			lua_setfield(L, -2, "set_volume");

			lua_pushcfunction(L, [](lua_State* L) -> int { MusicManager::get().music.setPitch(lua_tonumber(L, 1)); return 0; });
			lua_setfield(L, -2, "set_pitch");
		lua_setfield(L, -2, "music");
	lua_pop(L, 1);
}