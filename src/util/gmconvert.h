#pragma once

#ifdef GMCONVERT_IMPLEMENTATION
#undef GMCONVERT_IMPLEMENTATION

#ifndef GMC_EMBEDDED
#include <raylib.h>
#else
#include <SFML/Graphics.hpp>
#endif

#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include "vendor/json.hpp"

using namespace nlohmann;

class GameMakerProject;
class GMRoom;

struct GMColor {
	uint8_t value[4];
};

static GMColor NumberToColor(uint32_t number) {
	std::array<int, 8> remainders {};
	remainders[0] = static_cast<uint32_t>(number) % 16;
	double quotient = number / 16.0;

	for (int i = 1; i < 8; ++i) {
		remainders[i] = static_cast<uint32_t>(quotient) % 16;
		quotient /= 16.0;
	}

	GMColor c {};
	for (int i = 0; i < 4; ++i) {
		// get top and bottom bits from iteration to form num
		c.value[i] = static_cast<uint8_t>(remainders[(i * 2) + 1] * std::pow(16, 1) + remainders[i * 2] * std::pow(16, 0));
	}
	return c;
}

class GameMakerResource {
public:
	std::string name;
	virtual void read(json& j, GameMakerProject* proj) {
		name = j["name"];
	}
};

class GMDirectoryResource : public GameMakerResource {
public:
	std::filesystem::path directory;
};

class GameMakerProject {
private:
	void parse();
	void parseFile(const std::filesystem::path& p);
	void parsePath(const std::filesystem::path& p);

public:
	std::map<std::string, std::vector<std::string>> managed;
	std::map<std::string, std::vector<std::string>> lastManaged;
	std::filesystem::path directory, assetsPath;
	GameMakerProject(const std::filesystem::path& directory, const std::filesystem::path& output) : directory(directory) {
		assetsPath = output;
		parse();
	}
};

static std::thread resThread;
static std::filesystem::path usedPath;
static std::unordered_map<std::string, std::function<std::unique_ptr<GameMakerResource>()>> resourceMap;
static std::vector<std::string> messages;
static std::mutex mtx;
static bool complete = false;

#ifndef GMC_EMBEDDED
static std::filesystem::path lastPath;
std::filesystem::path usedOutputPath;
std::filesystem::path lastOutputPath;
#endif

static void AddMessage(const std::string& msg) {
#ifndef GMC_EMBEDDED
	std::lock_guard<std::mutex> guard(mtx);
	messages.push_back(msg);
#else
	std::cout << msg << "\n";
#endif
}

template <typename T>
static void AddResourceType(const std::string& t) {
	resourceMap[t] = []() {
		std::unique_ptr<GameMakerResource> resource = std::make_unique<T>();
		return std::move(resource);
	};
}

static std::unique_ptr<GameMakerResource> CreateResource(const std::string& res) {
	auto func = resourceMap.find(res);
	if (func == resourceMap.end()) {
		return nullptr;
	}
	else {
		return func->second();
	}
}

class GMRLayer : public GameMakerResource {
public:
	int depth;
	int gridX, gridY;
	bool visible;
	void read(json& j, GameMakerProject* proj) override {
		GameMakerResource::read(j, proj);
		visible = j["visible"].get<bool>();
		depth = j["depth"].get<int>();
		gridX = j["gridX"].get<int>();
		gridY = j["gridY"].get<int>();
	}

	virtual bool write(json& j, const std::filesystem::path& path, GMRoom* room) {
		j["name"] = name;
		j["depth"] = depth;
		j["grid_x"] = gridX;
		j["grid_y"] = gridY;
		return true;
	}
};

class GMTileSet : public GMDirectoryResource {
public:
	int separationX, separationY;
	int offsetX, offsetY;
	int tileWidth, tileHeight;
	int tileCount;
	std::string sprite;
	void write(GameMakerProject* proj) {
		std::filesystem::path path = proj->assetsPath / "tilesets" / (name + ".json");
		bool writeData = false;
		if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
			writeData = true;
		}
		else {
			std::filesystem::path origFile = directory / (name + ".yy");
			if (std::filesystem::last_write_time(origFile) > std::filesystem::last_write_time(path)) {
				writeData = true;
			}
		}

		if (writeData) {
			AddMessage("Writing " + name);
			json j = {
				{ "name", name },
				{ "offset_x", offsetX },
				{ "offset_y", offsetY },
				{ "separation_x", separationX },
				{ "separation_y", separationY },
				{ "tile_width", tileWidth },
				{ "tile_height", tileHeight },
				{ "tile_count", tileCount },
				{ "sprite", sprite }
			};
			if (sprite.empty()) {
				j["sprite"] = nullptr;
			}
			std::ofstream o(path);
			o << std::setw(4) << j;
		}
	}

	void read(json& j, GameMakerProject* proj) override {
		GMDirectoryResource::read(j, proj);
		proj->managed["tilesets"].push_back(name);
		separationX = j["tilehsep"];
		separationY = j["tilevsep"];
		offsetX = j["tilexoff"];
		offsetY = j["tileyoff"];
		tileWidth = j["tileWidth"];
		tileHeight = j["tileHeight"];
		tileCount = j["tile_count"];
		if (j.find("spriteId") == j.end()) {
			sprite = "";
		}
		else {
			sprite = j["spriteId"]["name"];
		}

		write(proj);
	}
};

class GMSound : public GMDirectoryResource {
public:
	float volume = 1.0f;
	int sampleRate = 44100;
	std::string extension;
	void read(json& j, GameMakerProject* proj) override {
		GMDirectoryResource::read(j, proj);
		proj->managed["sounds"].push_back(name);

		std::string soundFile = j["soundFile"].get<std::string>();
		size_t loc = soundFile.find_first_of(".");
		extension = soundFile.substr(loc);

		write(proj);
	}
	void write(GameMakerProject* proj) {
		std::filesystem::path path = proj->assetsPath / "sounds" / name;
		bool writeSound = false, writeData = false;

		if (!std::filesystem::exists(path)) {
			std::filesystem::create_directory(path);
			writeSound = true;
			writeData = true;
		}
		else {
			auto lastWrite = std::filesystem::last_write_time(path / std::string("sound" + extension));
			auto thisLW = std::filesystem::last_write_time(directory / std::string(name + extension));
			if (thisLW > lastWrite) {
				writeSound = true;
			}

			std::filesystem::path dataFile = path / "data.json";
			std::filesystem::path origFile = directory / (name + ".yy");
			if (std::filesystem::last_write_time(origFile) > std::filesystem::last_write_time(dataFile)) {
				writeData = true;
			}
		}

		if (writeData) {
			AddMessage("Writing " + name);
			json j = {
				{ "name", name },
				{ "volume", volume },
				{ "extension", extension },
				{ "sample_rate", sampleRate }
			};
			std::ofstream o(path / "data.json");
			o << std::setw(4) << j;
		}

		if (writeSound) {
			AddMessage("Writing sound " + name);
			std::filesystem::remove(path / std::string("sound" + extension));
			std::filesystem::copy_file(directory / std::string(name + extension), path / std::string("sound" + extension));
		}
	}
};

class GMSprite : public GMDirectoryResource {
public:
	struct Frame {
	public:
		std::filesystem::path path;
	};
	std::vector<std::unique_ptr<Frame>> frames;
	int originX;
	int originY;
	int width;
	int height;
	int bboxLeft;
	int bboxRight;
	int bboxTop;
	int bboxBottom;
	void read(json& j, GameMakerProject* proj) override {
		GMDirectoryResource::read(j, proj);
		proj->managed["sprites"].push_back(name);

		originX = j["sequence"]["xorigin"];
		originY = j["sequence"]["yorigin"];
		width = j["width"];
		height = j["height"];
		bboxLeft = j["bbox_left"];
		bboxRight = j["bbox_right"];
		bboxTop = j["bbox_top"];
		bboxBottom = j["bbox_bottom"];

		for (auto& f : j["frames"]) {
			std::unique_ptr frame = std::make_unique<Frame>();
			frame->path = directory / (f["name"].get<std::string>() + ".png");
			frames.push_back(std::move(frame));
		}

		write(proj);
	}

	void write(GameMakerProject* proj) {
		std::filesystem::path path = proj->assetsPath / "sprites" / name;
		bool writeFrames = false;
		bool writeData = false;
		if (!std::filesystem::exists(path)) {
			std::filesystem::create_directory(path);
			writeFrames = true;
			writeData = true;
		}
		else {
			std::filesystem::path framesFile = path / "frames.png";
			auto lastWrite = std::filesystem::last_write_time(framesFile);
			for (int i = 0; i < frames.size(); ++i) {
				auto thisLW = std::filesystem::last_write_time(frames[i]->path);
				if (thisLW > lastWrite) {
					writeFrames = true;
				}
			}

			std::filesystem::path dataFile = path / "data.json";
			std::filesystem::path origFile = directory / (name + ".yy");
			if (std::filesystem::last_write_time(origFile) > std::filesystem::last_write_time(dataFile)) {
				writeData = true;
			}
		}

		if (writeData) {
			AddMessage("Writing " + name);
			json j = {
				// { "name", name },
				{ "size", { width, height } },
				{ "origin", { originX, originY } },
				{ "hitbox", { bboxLeft, bboxTop, bboxRight, bboxBottom } }
			};
			// j["frames"] = frames.size();
			std::ofstream o(path / "data.json");
			o << std::setw(4) << j;
		}

		if (writeFrames) {
#ifdef GMC_EMBEDDED
            sf::Image img (sf::Vector2u { static_cast<unsigned int>(width * frames.size()), static_cast<unsigned int>(height) }, sf::Color::Transparent);
			for (int i = 0; i < frames.size(); ++i) {
                sf::Image frame(frames[i]->path.string());
                bool copied = img.copy(frame, sf::Vector2u { static_cast<unsigned int>(i * (float)width), 0 }, {}, true);
			}
			std::filesystem::path framePng = path / "frames.png";
            bool saved = img.saveToFile(framePng);
#else
			Image image = GenImageColor(width * frames.size(), height, BLANK);
			for (int i = 0; i < frames.size(); ++i) {
				Image frame = LoadImage(frames[i]->path.string().c_str());
				ImageDraw(&image, frame, { 0, 0, (float)width, (float)height }, { i * (float)width, 0, (float)width, (float)height }, WHITE);
				UnloadImage(frame);
			}
			std::filesystem::path framePng = path / "frames.png";
			ExportImage(image, framePng.string().c_str());
			UnloadImage(image);
#endif
		}
	}
};

class GMObject : public GMDirectoryResource {
private:
	bool doWrite = false;
public:
	std::string sprite;
	bool visible;
	std::string parent;
	json properties;

	void read(json& j, GameMakerProject* proj) override {
		GMDirectoryResource::read(j, proj);
		doWrite = true;

		proj->managed["objects"].push_back(name);

		std::filesystem::path path = proj->assetsPath / "objects" / (name + ".json");
		if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
			std::filesystem::path original = directory / (name + ".yy");
			auto lastWrite = std::filesystem::last_write_time(path);
			auto lastWriteOrig = std::filesystem::last_write_time(original);
			if (lastWriteOrig > lastWrite) {
				doWrite = true;
			}
			else {
				return;
			}
		}

		visible = j["visible"];

		if (j["spriteId"].is_null()) {
			sprite = "";
		}
		else {
			sprite = j["spriteId"]["name"];
		}

		if (j["parentObjectId"].is_null()) {
			parent = "";
		}
		else {
			parent = j["parentObjectId"]["name"];
		}

		for (auto& p : j["properties"]) {
			json object = json::object();
			object["name"] = p["name"];
			if (p.contains("resource")) {
				if (p["resource"].contains("path")) {
					object["value"] = p["resource"]["name"];
					std::string dir = p["resource"]["path"].get<std::string>();
					dir.erase(dir.find_first_of("/"));
					object["directory"] = dir;
				}
			}
			else if (p.find("value") != p.end()) {
				int t = p["varType"];
				object["type"] = t;
				if (t == 0) {
					object["value"] = std::stof(p["value"].get<std::string>());
				}
				else if (t == 1) {
					object["value"] = std::stoi(p["value"].get<std::string>());
				}
				else if (t == 3) {
					object["value"] = (p["value"] == "true" || p["value"] == "True") ? true : false;
				}
				else {
					object["value"] = p["value"].get<std::string>();
				}
			}
			properties.push_back(object);
		}

		write(proj);
	}

	void write(GameMakerProject* proj) {
		if (!doWrite) {
			return;
		}

		AddMessage("Writing " + name);

		json j = json::object();

		j["visible"] = visible;

		if (sprite.empty()) {
			j["sprite"] = nullptr;
		}
		else {
			j["sprite"] = sprite;
		}
		if (parent.empty()) {
			j["parent"] = nullptr;
		}
		else {
			j["parent"] = parent;
		}

		j["properties"] = properties;

		std::ofstream o(proj->assetsPath / "objects" / std::string(name + ".json"));
		o << std::setw(4) << j;
	}
};

class GMRoom : public GMDirectoryResource {
private:
	bool doWrite = false;
public:
	struct RoomSettings {
		int width;
		int height;
		bool persistent;
	} settings;
	std::vector<std::unique_ptr<GMRLayer>> layers;
	void read(json& j, GameMakerProject* proj) override {
		doWrite = true;
		GMDirectoryResource::read(j, proj);
		proj->managed["rooms"].push_back(name);
		std::filesystem::path goalPath = proj->assetsPath / "rooms" / name / "data.json";
		if (std::filesystem::exists(goalPath)) {
			auto lastWrite = std::filesystem::last_write_time(goalPath);
			auto lastWriteOrig = std::filesystem::last_write_time(directory / (name + ".yy"));
			if (lastWriteOrig < lastWrite) {
				doWrite = false;
				return;
			}
			else {
				AddMessage("Writing " + name);
			}
		}
		auto& jSettings = j["roomSettings"];
		settings.width = jSettings["Width"];
		settings.height = jSettings["Height"];
		settings.persistent = jSettings["persistent"];

		for (auto& lData : j["layers"]) {
			std::unique_ptr origRes = CreateResource(lData["resourceType"].get<std::string>());
			if (origRes == nullptr) continue;

			std::unique_ptr<GMRLayer> layer(static_cast<GMRLayer*>(origRes.release()));
			layer->read(lData, proj);

			layers.push_back(std::move(layer));
		}

		write(proj);
	}

	void write(GameMakerProject* proj) {
		if (!doWrite) {
			return;
		}

		std::filesystem::path path = proj->assetsPath / "rooms" / name;
		if (!std::filesystem::exists(path)) {
			std::filesystem::create_directory(path);
		}
		else if (!std::filesystem::is_directory(path)) {
			std::filesystem::remove(path);
			std::filesystem::create_directory(path);
		}

		for (const auto& entry : std::filesystem::directory_iterator(path))
			std::filesystem::remove_all(entry.path());

		json j;
		j["name"] = name;
		j["room_settings"] = {
			{ "width", settings.width },
			{ "height", settings.height },
			{ "persistent", settings.persistent },
		};
		j["layers"] = json::array();
		for (int i = 0; i < layers.size(); ++i) {
			json o = json::object();
			auto& layer = layers[i];
			if (layer->write(o, path, this)) {
				j["layers"].push_back(o);
			}
		}

		std::ofstream o(path / ("data.json"));
		o << std::setw(4) << j << std::endl;
	}
};

class GMRTileLayer : public GMRLayer {
public:
	std::vector<int32_t> tileData;
	std::string tileset;
	int serializeWidth, serializeHeight;
	const int nilValue = -2147483648;
	void decompressTiles(json& compData) {
		tileData.clear();
		tileData.reserve(serializeWidth * serializeHeight);
		auto v = compData.get<std::vector<int>>();
		int size = v.size();


		for (int i = 0; i < size;) {
			int value = v[i++];
			if (value == nilValue) { // nil tile
				value = 0;
			}

			// Start a value train
			if (value >= 0) {
				while (true) {
					// stay in bounds
					if (i >= size) {
						break;
					}

					int nextValue = v[i++];
					if (nextValue == nilValue) { // nil tile
						nextValue = 0;
					}

					if (nextValue >= 0) {
						tileData.push_back(nextValue);
					}
					else {
						value = nextValue;
						break;
					}
				}
			}

			// Negative value is count
			if (value < 0) {
				// stay in bounds
				if (i >= size) {
					break;
				}

				int repeatValue = v[i++];
				if (repeatValue == nilValue) { // nil tile
					repeatValue = 0;
				}

				for (int i = 0; i < -value; ++i) {
					tileData.push_back(repeatValue);
				}
			}
		}
	}
	void read(json& j, GameMakerProject* proj) override {
		GMRLayer::read(j, proj);
		auto& tData = j["tiles"];
		serializeWidth = tData["SerialiseWidth"];
		serializeHeight = tData["SerialiseHeight"];
		if (!j["tilesetId"].is_null()) {
			tileset = j["tilesetId"]["name"];
		}
		else {
			tileset = "";
		}

		if (tData.find("TileDataFormat") == tData.end()) {
			tileData = tData["TileSerialiseData"].get<std::vector<int32_t>>();
			for (auto& t : tileData) {
				if (t == nilValue) {
					t = 0;
				}
			}
		}
		else {
			auto& compData = tData["TileCompressedData"];
			decompressTiles(compData);
		}

	}
	bool write(json& j, const std::filesystem::path& path, GMRoom* room) override {
		GMRLayer::write(j, path, room);
		j["layer_type"] = "tiles";
		j["width"] = serializeWidth;
		j["height"] = serializeHeight;
		j["visible"] = visible;
		if (tileset.empty()) {
			j["tileset"] = nullptr;
		}
		else {
			j["tileset"] = tileset;
		}
		j["tiles"] = tileData;
		return true;
	}
};

class GMRInstanceLayer : public GMRLayer {
public:
	struct Instance {
		std::string object;
		std::string uuid;
		float rotation;
		float scaleX;
		float scaleY;
		float x;
		float y;
		float imageIndex;
		float imageSpeed;
		bool hasCreationCode;
		json properties;
		GMColor color;
	};

	std::vector<std::unique_ptr<Instance>> instances;

	void read(json& j, GameMakerProject* proj) override {
		GMRLayer::read(j, proj);

		auto& jInstances = j["instances"];
		int count = jInstances.size();

		for (auto& jInst : jInstances) {
			if (jInst["ignore"]) {
				continue;
			}
			std::unique_ptr<Instance> instance = std::make_unique<Instance>();
			instance->object = jInst["objectId"]["name"];
			instance->uuid = jInst["name"];
			instance->rotation = jInst["rotation"];
			instance->scaleX = jInst["scaleX"];
			instance->scaleY = jInst["scaleY"];
			instance->x = jInst["x"];
			instance->y = jInst["y"];
			instance->imageIndex = jInst["imageIndex"];
			instance->hasCreationCode = jInst["hasCreationCode"];
			instance->imageSpeed = jInst["imageSpeed"];
			instance->imageSpeed = jInst["imageSpeed"];
			instance->color = NumberToColor(jInst["colour"].get<uint32_t>());
			instance->properties = json::array();
			for (auto& p : jInst["properties"]) {
				json object = json::object();

				object = { { p["propertyId"]["name"], p["value"] } };
				
				instance->properties.push_back(object);
			}
			instances.push_back(std::move(instance));
		}
	}

	bool write(json& j, const std::filesystem::path& path, GMRoom* room) override {
		if (instances.empty()) {
			return false;
		}
		GMRLayer::write(j, path, room);
		j["layer_type"] = "instances";
		j["instances"] = json::array();
		for (auto& i : instances) {
			json o = json::object();
			o["object_index"] = i->object;
			if (i->rotation != 0) {
				o["rotation"] = i->rotation;
			}
			if (i->scaleX != 1) {
				o["scale_x"] = i->scaleX;
			}
			if (i->scaleY != 1) {
				o["scale_y"] = i->scaleY;
			}
			o["x"] = i->x;
			o["y"] = i->y;
			o["image_index"] = i->imageIndex;
			o["image_speed"] = i->imageSpeed;
			o["properties"] = i->properties;
			o["color"] = { i->color.value[0], i->color.value[1], i->color.value[2], i->color.value[3] };
			j["instances"].push_back(o);

			if (i->hasCreationCode) {
				std::filesystem::path cc = room->directory / std::string("InstanceCreationCode_" + i->uuid + ".gml");

				std::ifstream file(cc);
				std::string s;
				std::vector<std::string> lines;
				lines.emplace_back("local instance = ...");
				lines.emplace_back("function instance:creation_code()");

				while (getline(file, s)) {
					s.insert(0, "\t");
					lines.push_back(s);
				}

				std::filesystem::path output = path / std::string(i->uuid + "_cc.lua");
				if (std::filesystem::exists(output)) {
					std::filesystem::remove(output);
				}
				lines.emplace_back("end");
				std::ofstream o(output);
				for (auto& l : lines) {
					o << l << std::endl;
				}
			}
		}
		return true;
	}
};

class GMRBackgroundLayer : public GMRLayer {
public:
	GMColor color;
	float speedX;
	float speedY;
	bool tiledX;
	bool tiledY;
	float x;
	float y;
	std::string sprite;
	void read(json& j, GameMakerProject* proj) override {
		GMRLayer::read(j, proj);
		color = NumberToColor(j["colour"].get<uint32_t>());
		x = j["x"];
		y = j["y"];
		tiledX = j["htiled"];
		tiledY = j["vtiled"];
		speedX = j["hspeed"];
		speedY = j["vspeed"];
		visible = j["visible"];
		if (j["spriteId"].is_null()) {
			sprite = "";
		}
		else {
			sprite = j["spriteId"]["name"];
		}
	}

	bool write(json& j, const std::filesystem::path& path, GMRoom* room) override {
		GMRLayer::write(j, path, room);
		j["layer_type"] = "background";
		j["tiled_x"] = tiledX;
		j["tiled_y"] = tiledY;
		j["speed_x"] = speedX;
		j["speed_y"] = speedY;
		j["x"] = x;
		j["y"] = y;
		j["visible"] = visible;
		if (sprite.empty()) {
			j["sprite"] = nullptr;
		}
		else {
			j["sprite"] = sprite;
		}
		j["color"] = { color.value[0], color.value[1], color.value[2], color.value[3] };
		return true;
	}
};

void GameMakerProject::parseFile(const std::filesystem::path& p) {
	std::ifstream f(p);
	json j = json::parse(f, nullptr, true, true);
	const std::string resource = j["resourceType"];

	std::unique_ptr res = CreateResource(resource);
	if (res == nullptr) {
		return;
	}

	std::unique_ptr<GMDirectoryResource> dirRes = std::unique_ptr<GMDirectoryResource>(static_cast<GMDirectoryResource*>(res.release()));
	dirRes->directory = p.parent_path();
	dirRes->read(j, this);
}

void GameMakerProject::parsePath(const std::filesystem::path& p) {
	for (auto& it : std::filesystem::directory_iterator(p)) {
		if (it.is_directory()) {
			// Create file name from upper directory
			std::string fName = it.path().filename().string();
			// Construct file using the file name
			std::filesystem::path jsonPath = it.path() / (fName + ".yy");
			// Parse file if it exists
			if (std::filesystem::exists(jsonPath)) {
				parseFile(jsonPath);
			}
		}
	}
}

void GameMakerProject::parse() {
	AddMessage("Starting GMConvert update...");

	auto start = std::chrono::high_resolution_clock::now();

	std::vector<std::string> paths = { "rooms", "objects", "sprites", "tilesets", "sounds" };

	if (!std::filesystem::exists(assetsPath)) {
		std::filesystem::create_directory(assetsPath);
	}
	else if (std::filesystem::exists(assetsPath / "assets.json")) {
		std::ifstream i(assetsPath / "assets.json");
		json j = json::parse(i);
		for (auto& obj : j.get<json::object_t>()) {
			lastManaged[obj.first] = obj.second.get<std::vector<std::string>>();
		}
	}

	for (auto& p : paths) {
		const std::filesystem::path construct = assetsPath / p;
		if (!std::filesystem::exists(construct)) {
			std::filesystem::create_directory(construct);
		}
	}

	for (auto& it : std::filesystem::directory_iterator(directory)) {
		if (it.is_directory()) {
			parsePath(it.path());
		}
	}

	for (auto& [k, v] : lastManaged) {
		auto& objs = managed[k];
		for (auto& str : v) {
			auto it = std::find(objs.begin(), objs.end(), str);
			if (it == objs.end()) {
				std::filesystem::remove(assetsPath / k / std::string(str + ".json"));
				std::filesystem::remove_all(assetsPath / k / std::string(str));
			}
		}
	}

	json j = json::object();
	for (auto& [k, v] : managed) {
		j[k] = v;
	}
	std::ofstream managedObjects(assetsPath / "managed.json");
	managedObjects << std::setw(4) << j;

	auto end = std::chrono::high_resolution_clock::now();

	auto count = (end - start).count() / 1000000;
	double seconds = static_cast<double>(count) / 1000.0;
	std::string secStr = std::to_string(seconds);
	secStr.erase(secStr.find_last_not_of('0') + 1, std::string::npos);
	AddMessage("Finished! (Took " + secStr + "s)");
#ifndef GMC_EMBEDDED
	AddMessage("Closing program... _");
#endif
	{
		std::lock_guard<std::mutex> guard(mtx);
		complete = true;
	}
}

#ifndef GMC_EMBEDDED
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

void RunWindow() {
	int screenWidth = 640;
	int screenHeight = 480;
	
	SetTraceLogLevel(LOG_NONE);

	InitWindow(screenWidth, screenHeight, "GMConvert");
	SetExitKey(0);

	SetTargetFPS(60);

	int state = 0;
	float timer = 0;
	bool close = false;

	bool hadLastPath = false;

	if (std::filesystem::exists("gmconv.session")) {
		std::ifstream i("gmconv.session");
		json j = json::parse(i);
		lastPath = std::filesystem::path(j["previous"]["input"].get<std::string>());
		lastOutputPath = std::filesystem::path(j["previous"]["output"].get<std::string>());
		hadLastPath = true;
	}

	while (!close) {
		if (WindowShouldClose()) {
			close = true;
		}

		Color bgColor = { 35, 35, 35, 255 };

		if (state == 0 || state == 1) {
			if (IsFileDropped()) {

				FilePathList droppedFiles = LoadDroppedFiles();

				if (droppedFiles.count == 1) {
					std::string stringP = droppedFiles.paths[0];

					// Dragged in input folder
					if (state == 0) {
						usedPath = stringP;
						state = 1;
					}
					// Dragged in output folder
					else if (state == 1) {
						usedOutputPath = stringP;
						resThread = std::thread([p = usedPath, op = usedOutputPath]() { GameMakerProject gmp(p, op); });
						state = 2;
					}
				}

				UnloadDroppedFiles(droppedFiles);
			}

			BeginDrawing();
			ClearBackground(bgColor);

			std::string txt = "Drag project folder into window";
			if (state == 1) {
				txt = "Drag output folder into window";
			}
			float w = MeasureText(txt.c_str(), 20);
			DrawText(txt.c_str(), (float)GetScreenWidth() / 2 - w / 2, (GetScreenHeight() / 2) - ((!hadLastPath || state == 1) ? 20 : 40), 20, WHITE);

			if (state == 0 && hadLastPath) {
				std::string lastTxt = "Or use last (" + lastPath.string() + ")";
				float lastTxtWidth = MeasureText(lastTxt.c_str(), 10);
				if (GuiButton({ (GetScreenWidth() / 2 - lastTxtWidth / 2) - 10, ((float)GetScreenHeight() / 2) + 0, lastTxtWidth + 20, 30 }, lastTxt.c_str())) {
					usedPath = lastPath;
					usedOutputPath = lastOutputPath;
					resThread = std::thread([p = usedPath, op = usedOutputPath]() { GameMakerProject gmp(p, op); });
					state = 2;
				}
			}
		}

		if (state == 2) {
			{
				bool thisComplete = false;
				{
					std::lock_guard<std::mutex> guard(mtx);
					thisComplete = complete;
				}
				if (thisComplete) {
					float timeFrom = (3 - timer) + 1;
					std::string& closing = messages.back();
					closing[closing.length() - 1] = std::to_string(timeFrom)[0];
					timer += GetFrameTime();
					if (timer >= 3) {
						close = true;
					}
				}
			}

			BeginDrawing();
			ClearBackground(bgColor);

			{
				std::lock_guard<std::mutex> guard(mtx);
				int py = 0;

				for (int i = messages.size() - 1; i >= 0; --i) {
					std::string& m = messages[i];
					int _i = messages.size() - i;
					DrawText(m.c_str(), 10, screenHeight - (_i * 15), 10, WHITE);
				}
			}
		}

		EndDrawing();
	}
}

void SaveSession() {
	json meta;
	meta["previous"] = {
		{ "input", usedPath.string() },
		{ "output", usedOutputPath.string() }
	};
	std::ofstream o("gmconv.session");
	o << std::setw(4) << meta;
}
#endif

void AddResourceTypes() {
	// Base types
	AddResourceType<GMRoom>("GMRoom");
	AddResourceType<GMObject>("GMObject");
	AddResourceType<GMSprite>("GMSprite");
	AddResourceType<GMTileSet>("GMTileSet");
	AddResourceType<GMSound>("GMSound");

	// Layers
	AddResourceType<GMRBackgroundLayer>("GMRBackgroundLayer");
	AddResourceType<GMRInstanceLayer>("GMRInstanceLayer");
	AddResourceType<GMRTileLayer>("GMRTileLayer");
}

#ifdef GMC_EMBEDDED
void GMConvert(const std::filesystem::path& usedPath, const std::filesystem::path& usedOutputPath) {
#else
void GMConvert() {
#endif
	AddResourceTypes();

#ifndef GMC_EMBEDDED
	RunWindow();
	CloseWindow();
	SaveSession();
#else
	resThread = std::thread([p = usedPath, op = usedOutputPath]() { GameMakerProject gmp(p, op); });
#endif

	if (resThread.joinable()) {
		resThread.join();
	}
}

#endif