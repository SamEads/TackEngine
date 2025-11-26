#include "keys.h"

void Keys::update(bool windowFocused) {
	for (int i = 0; i < keys.size(); ++i)
		keysLast[i] = keys[i];

	if (!windowFocused) {
		for (int i = 0; i < keys.size(); ++i)
			keys[i] = false;
	}
	else for (int i = 0; i < keys.size(); ++i) {
		keys[i] = sf::Keyboard::isKeyPressed((sf::Keyboard::Scancode)i);
	}
}

bool Keys::pressed(sf::Keyboard::Scancode key) { return keys[(int)key] && !keysLast[(int)key]; }
bool Keys::held(sf::Keyboard::Scancode key) { return keys[(int)key]; }
bool Keys::released(sf::Keyboard::Scancode key) { return keysLast[(int)key] && !keys[(int)key]; }

void Keys::initializeLua(sol::state& lua) {
    using namespace sf::Keyboard;

    sol::table engineEnv = lua["TE"];
    sol::table keyboardModule = engineEnv.create_named("keyboard");

    keyboardModule["check"] = [this](Scancode key) { return held(key); };
    keyboardModule["pressed"] = [this](Scancode key) { return pressed(key); };
    keyboardModule["released"] = [this](Scancode key) { return released(key); };

	engineEnv.new_enum("Key",
        "unknown", Scancode::Unknown,
        "a", Scancode::A,
        "b", Scancode::B,
        "c", Scancode::C,
        "d", Scancode::D,
        "e", Scancode::E,
        "f", Scancode::F,
        "g", Scancode::G,
        "h", Scancode::H,
        "i", Scancode::I,
        "j", Scancode::J,
        "k", Scancode::K,
        "l", Scancode::L,
        "m", Scancode::M,
        "n", Scancode::N,
        "o", Scancode::O,
        "p", Scancode::P,
        "q", Scancode::Q,
        "r", Scancode::R,
        "s", Scancode::S,
        "t", Scancode::T,
        "u", Scancode::U,
        "v", Scancode::V,
        "w", Scancode::W,
        "x", Scancode::X,
        "y", Scancode::Y,
        "z", Scancode::Z,
        "one", Scancode::Num1,
        "two", Scancode::Num2,
        "three", Scancode::Num3,
        "four", Scancode::Num4,
        "five", Scancode::Num5,
        "six", Scancode::Num6,
        "seven", Scancode::Num7,
        "eight", Scancode::Num8,
        "nine", Scancode::Num9,
        "ten", Scancode::Num0,
        "enter", Scancode::Enter,
        "escape", Scancode::Escape,
        "backspace", Scancode::Backspace,
        "tab", Scancode::Tab,
        "space", Scancode::Space,
        "hyphen", Scancode::Hyphen,
        "equal", Scancode::Equal,
        "lbracket", Scancode::LBracket,
        "rbracket", Scancode::RBracket,
        "backslash", Scancode::Backslash,
        "semicolon", Scancode::Semicolon,
        "apostrophe", Scancode::Apostrophe,
        "grave", Scancode::Grave,
        "comma", Scancode::Comma,
        "period", Scancode::Period,
        "slash", Scancode::Slash,
        "f1", Scancode::F1,
        "f2", Scancode::F2,
        "f3", Scancode::F3,
        "f4", Scancode::F4,
        "f5", Scancode::F5,
        "f6", Scancode::F6,
        "f7", Scancode::F7,
        "f8", Scancode::F8,
        "f9", Scancode::F9,
        "f10", Scancode::F10,
        "f11", Scancode::F11,
        "f12", Scancode::F12,
        "f13", Scancode::F13,
        "f14", Scancode::F14,
        "f15", Scancode::F15,
        "f16", Scancode::F16,
        "f17", Scancode::F17,
        "f18", Scancode::F18,
        "f19", Scancode::F19,
        "f20", Scancode::F20,
        "f21", Scancode::F21,
        "f22", Scancode::F22,
        "f23", Scancode::F23,
        "f24", Scancode::F24,
        "capslock", Scancode::CapsLock,       //!< Keyboard Caps %Lock key
        "printscreen", Scancode::PrintScreen,    //!< Keyboard Print Screen key
        "scrolllock", Scancode::ScrollLock,     //!< Keyboard Scroll %Lock key
        "pause", Scancode::Pause,          //!< Keyboard Pause key
        "insert", Scancode::Insert,         //!< Keyboard Insert key
        "home", Scancode::Home,           //!< Keyboard Home key
        "pageup", Scancode::PageUp,         //!< Keyboard Page Up key
        "delete", Scancode::Delete,         //!< Keyboard Delete Forward key
        "end", Scancode::End,            //!< Keyboard End key
        "pagedown", Scancode::PageDown,       //!< Keyboard Page Down key
        "right", Scancode::Right,          //!< Keyboard Right Arrow key
        "left", Scancode::Left,           //!< Keyboard Left Arrow key
        "down", Scancode::Down,           //!< Keyboard Down Arrow key
        "up", Scancode::Up,             //!< Keyboard Up Arrow key
        "numlock", Scancode::NumLock,        //!< Keypad Num %Lock and Clear key
        "npdivide", Scancode::NumpadDivide,   //!< Keypad / key
        "npdivide", Scancode::NumpadMultiply, //!< Keypad * key
        "npminus", Scancode::NumpadMinus,    //!< Keypad - key
        "npplus", Scancode::NumpadPlus,     //!< Keypad + key
        "npequal", Scancode::NumpadEqual,    //!< keypad = key
        "npenter", Scancode::NumpadEnter,    //!< Keypad Enter/Return key
        "npdecimal", Scancode::NumpadDecimal,  //!< Keypad . and Delete key
        "npone", Scancode::Numpad1,        //!< Keypad 1 and End key
        "nptwo", Scancode::Numpad2,        //!< Keypad 2 and Down Arrow key
        "npthree", Scancode::Numpad3,        //!< Keypad 3 and Page Down key
        "npfour", Scancode::Numpad4,        //!< Keypad 4 and Left Arrow key
        "npfive", Scancode::Numpad5,        //!< Keypad 5 key
        "npsix", Scancode::Numpad6,        //!< Keypad 6 and Right Arrow key
        "npseven", Scancode::Numpad7,        //!< Keypad 7 and Home key
        "npeight", Scancode::Numpad8,        //!< Keypad 8 and Up Arrow key
        "npnine", Scancode::Numpad9,        //!< Keypad 9 and Page Up key
        "npten", Scancode::Numpad0,        //!< Keypad 0 and Insert key
        "backslashalt", Scancode::NonUsBackslash,     //!< Keyboard Non-US \ and | key
        "application", Scancode::Application,        //!< Keyboard Application key
        "execute", Scancode::Execute,            //!< Keyboard Execute key
        "modechange", Scancode::ModeChange,         //!< Keyboard Mode Change key
        "help", Scancode::Help,               //!< Keyboard Help key
        "menu", Scancode::Menu,               //!< Keyboard Menu key
        "select", Scancode::Select,             //!< Keyboard Select key
        "redo", Scancode::Redo,               //!< Keyboard Redo key
        "undo", Scancode::Undo,               //!< Keyboard Undo key
        "cut", Scancode::Cut,                //!< Keyboard Cut key
        "copy", Scancode::Copy,               //!< Keyboard Copy key
        "paste", Scancode::Paste,              //!< Keyboard Paste key
        "volumemute", Scancode::VolumeMute,         //!< Keyboard Volume Mute key
        "volumeup", Scancode::VolumeUp,           //!< Keyboard Volume Up key
        "volumedown", Scancode::VolumeDown,         //!< Keyboard Volume Down key
        "playpause", Scancode::MediaPlayPause,     //!< Keyboard Media Play Pause key
        "stop", Scancode::MediaStop,          //!< Keyboard Media Stop key
        "nexttrack", Scancode::MediaNextTrack,     //!< Keyboard Media Next Track key
        "prevtrack", Scancode::MediaPreviousTrack, //!< Keyboard Media Previous Track key
        "lcontrol", Scancode::LControl,           //!< Keyboard Left Control key
        "lshift", Scancode::LShift,             //!< Keyboard Left Shift key
        "lalt", Scancode::LAlt,               //!< Keyboard Left Alt key
        "lsystem", Scancode::LSystem,            //!< Keyboard Left System key
        "rcontrol", Scancode::RControl,           //!< Keyboard Right Control key
        "rshift", Scancode::RShift,             //!< Keyboard Right Shift key
        "ralt", Scancode::RAlt,               //!< Keyboard Right Alt key
        "rsystem", Scancode::RSystem,            //!< Keyboard Right System key
        "back", Scancode::Back,               //!< Keyboard Back key
        "forward", Scancode::Forward,            //!< Keyboard Forward key
        "refresh", Scancode::Refresh,            //!< Keyboard Refresh key
        "stop", Scancode::Stop,               //!< Keyboard Stop key
        "search", Scancode::Search,             //!< Keyboard Search key
        "favorites", Scancode::Favorites,          //!< Keyboard Favorites key
        "homepage", Scancode::HomePage,           //!< Keyboard Home Page key
        "launchapplication1", Scancode::LaunchApplication1, //!< Keyboard Launch Application 1 key
        "launchapplication2", Scancode::LaunchApplication2, //!< Keyboard Launch Application 2 key
        "launchmail", Scancode::LaunchMail,         //!< Keyboard Launch Mail key
        "launchmediaselect", Scancode::LaunchMediaSelect  //!< Keyboard Launch Media Select key
	);
}