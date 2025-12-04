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

void Keys::initializeLua(LuaState L) {
    using namespace sf::Keyboard;

    lua_getglobal(L, ENGINE_ENV);

        #define REGKEY(str, code) lua_pushnumber(L, static_cast<int>(code)); lua_setfield(L, -2, str);

        lua_newtable(L);
            REGKEY("a", Scancode::A)
            REGKEY("b", Scancode::B)
            REGKEY("c", Scancode::C)
            REGKEY("d", Scancode::D)
            REGKEY("e", Scancode::E)
            REGKEY("f", Scancode::F)
            REGKEY("g", Scancode::G)
            REGKEY("h", Scancode::H)
            REGKEY("i", Scancode::I)
            REGKEY("j", Scancode::J)
            REGKEY("k", Scancode::K)
            REGKEY("l", Scancode::L)
            REGKEY("m", Scancode::M)
            REGKEY("n", Scancode::N)
            REGKEY("o", Scancode::O)
            REGKEY("p", Scancode::P)
            REGKEY("q", Scancode::Q)
            REGKEY("r", Scancode::R)
            REGKEY("s", Scancode::S)
            REGKEY("t", Scancode::T)
            REGKEY("u", Scancode::U)
            REGKEY("v", Scancode::V)
            REGKEY("w", Scancode::W)
            REGKEY("x", Scancode::X)
            REGKEY("y", Scancode::Y)
            REGKEY("z", Scancode::Z)
            REGKEY("one", Scancode::Num1)
            REGKEY("two", Scancode::Num2)
            REGKEY("three", Scancode::Num3)
            REGKEY("four", Scancode::Num4)
            REGKEY("five", Scancode::Num5)
            REGKEY("six", Scancode::Num6)
            REGKEY("seven", Scancode::Num7)
            REGKEY("eight", Scancode::Num8)
            REGKEY("nine", Scancode::Num9)
            REGKEY("ten", Scancode::Num0)
            REGKEY("enter", Scancode::Enter)
            REGKEY("escape", Scancode::Escape)
            REGKEY("backspace", Scancode::Backspace)
            REGKEY("tab", Scancode::Tab)
            REGKEY("space", Scancode::Space)
            REGKEY("hyphen", Scancode::Hyphen)
            REGKEY("equal", Scancode::Equal)
            REGKEY("lbracket", Scancode::LBracket)
            REGKEY("rbracket", Scancode::RBracket)
            REGKEY("backslash", Scancode::Backslash)
            REGKEY("semicolon", Scancode::Semicolon)
            REGKEY("apostrophe", Scancode::Apostrophe)
            REGKEY("grave", Scancode::Grave)
            REGKEY("comma", Scancode::Comma)
            REGKEY("period", Scancode::Period)
            REGKEY("slash", Scancode::Slash)
            REGKEY("f1", Scancode::F1)
            REGKEY("f2", Scancode::F2)
            REGKEY("f3", Scancode::F3)
            REGKEY("f4", Scancode::F4)
            REGKEY("f5", Scancode::F5)
            REGKEY("f6", Scancode::F6)
            REGKEY("f7", Scancode::F7)
            REGKEY("f8", Scancode::F8)
            REGKEY("f9", Scancode::F9)
            REGKEY("f10", Scancode::F10)
            REGKEY("f11", Scancode::F11)
            REGKEY("f12", Scancode::F12)
            REGKEY("f13", Scancode::F13)
            REGKEY("f14", Scancode::F14)
            REGKEY("f15", Scancode::F15)
            REGKEY("f16", Scancode::F16)
            REGKEY("f17", Scancode::F17)
            REGKEY("f18", Scancode::F18)
            REGKEY("f19", Scancode::F19)
            REGKEY("f20", Scancode::F20)
            REGKEY("f21", Scancode::F21)
            REGKEY("f22", Scancode::F22)
            REGKEY("f23", Scancode::F23)
            REGKEY("f24", Scancode::F24)
            REGKEY("capslock", Scancode::CapsLock)       //!< Keyboard Caps %Lock key
            REGKEY("printscreen", Scancode::PrintScreen)    //!< Keyboard Print Screen key
            REGKEY("scrolllock", Scancode::ScrollLock)     //!< Keyboard Scroll %Lock key
            REGKEY("pause", Scancode::Pause)          //!< Keyboard Pause key
            REGKEY("insert", Scancode::Insert)         //!< Keyboard Insert key
            REGKEY("home", Scancode::Home)           //!< Keyboard Home key
            REGKEY("pageup", Scancode::PageUp)         //!< Keyboard Page Up key
            REGKEY("delete", Scancode::Delete)         //!< Keyboard Delete Forward key
            REGKEY("end", Scancode::End)            //!< Keyboard End key
            REGKEY("pagedown", Scancode::PageDown)       //!< Keyboard Page Down key
            REGKEY("right", Scancode::Right)          //!< Keyboard Right Arrow key
            REGKEY("left", Scancode::Left)           //!< Keyboard Left Arrow key
            REGKEY("down", Scancode::Down)           //!< Keyboard Down Arrow key
            REGKEY("up", Scancode::Up)             //!< Keyboard Up Arrow key
            REGKEY("numlock", Scancode::NumLock)        //!< Keypad Num %Lock and Clear key
            REGKEY("npdivide", Scancode::NumpadDivide)   //!< Keypad / key
            REGKEY("npdivide", Scancode::NumpadMultiply) //!< Keypad * key
            REGKEY("npminus", Scancode::NumpadMinus)    //!< Keypad - key
            REGKEY("npplus", Scancode::NumpadPlus)     //!< Keypad + key
            REGKEY("npequal", Scancode::NumpadEqual)    //!< keypad = key
            REGKEY("npenter", Scancode::NumpadEnter)    //!< Keypad Enter/Return key
            REGKEY("npdecimal", Scancode::NumpadDecimal)  //!< Keypad . and Delete key
            REGKEY("npone", Scancode::Numpad1)        //!< Keypad 1 and End key
            REGKEY("nptwo", Scancode::Numpad2)        //!< Keypad 2 and Down Arrow key
            REGKEY("npthree", Scancode::Numpad3)        //!< Keypad 3 and Page Down key
            REGKEY("npfour", Scancode::Numpad4)        //!< Keypad 4 and Left Arrow key
            REGKEY("npfive", Scancode::Numpad5)        //!< Keypad 5 key
            REGKEY("npsix", Scancode::Numpad6)        //!< Keypad 6 and Right Arrow key
            REGKEY("npseven", Scancode::Numpad7)        //!< Keypad 7 and Home key
            REGKEY("npeight", Scancode::Numpad8)        //!< Keypad 8 and Up Arrow key
            REGKEY("npnine", Scancode::Numpad9)        //!< Keypad 9 and Page Up key
            REGKEY("npten", Scancode::Numpad0)        //!< Keypad 0 and Insert key
            REGKEY("backslashalt", Scancode::NonUsBackslash)     //!< Keyboard Non-US \ and | key
            REGKEY("application", Scancode::Application)        //!< Keyboard Application key
            REGKEY("execute", Scancode::Execute)            //!< Keyboard Execute key
            REGKEY("modechange", Scancode::ModeChange)         //!< Keyboard Mode Change key
            REGKEY("help", Scancode::Help)               //!< Keyboard Help key
            REGKEY("menu", Scancode::Menu)               //!< Keyboard Menu key
            REGKEY("select", Scancode::Select)             //!< Keyboard Select key
            REGKEY("redo", Scancode::Redo)               //!< Keyboard Redo key
            REGKEY("undo", Scancode::Undo)               //!< Keyboard Undo key
            REGKEY("cut", Scancode::Cut)                //!< Keyboard Cut key
            REGKEY("copy", Scancode::Copy)               //!< Keyboard Copy key
            REGKEY("paste", Scancode::Paste)              //!< Keyboard Paste key
            REGKEY("volumemute", Scancode::VolumeMute)         //!< Keyboard Volume Mute key
            REGKEY("volumeup", Scancode::VolumeUp)           //!< Keyboard Volume Up key
            REGKEY("volumedown", Scancode::VolumeDown)         //!< Keyboard Volume Down key
            REGKEY("playpause", Scancode::MediaPlayPause)     //!< Keyboard Media Play Pause key
            REGKEY("stop", Scancode::MediaStop)          //!< Keyboard Media Stop key
            REGKEY("nexttrack", Scancode::MediaNextTrack)     //!< Keyboard Media Next Track key
            REGKEY("prevtrack", Scancode::MediaPreviousTrack) //!< Keyboard Media Previous Track key
            REGKEY("lcontrol", Scancode::LControl)           //!< Keyboard Left Control key
            REGKEY("lshift", Scancode::LShift)             //!< Keyboard Left Shift key
            REGKEY("lalt", Scancode::LAlt)               //!< Keyboard Left Alt key
            REGKEY("lsystem", Scancode::LSystem)            //!< Keyboard Left System key
            REGKEY("rcontrol", Scancode::RControl)           //!< Keyboard Right Control key
            REGKEY("rshift", Scancode::RShift)             //!< Keyboard Right Shift key
            REGKEY("ralt", Scancode::RAlt)               //!< Keyboard Right Alt key
            REGKEY("rsystem", Scancode::RSystem)            //!< Keyboard Right System key
            REGKEY("back", Scancode::Back)               //!< Keyboard Back key
            REGKEY("forward", Scancode::Forward)            //!< Keyboard Forward key
            REGKEY("refresh", Scancode::Refresh)            //!< Keyboard Refresh key
            REGKEY("stop", Scancode::Stop)               //!< Keyboard Stop key
            REGKEY("search", Scancode::Search)             //!< Keyboard Search key
            REGKEY("favorites", Scancode::Favorites)          //!< Keyboard Favorites key
            REGKEY("homepage", Scancode::HomePage)           //!< Keyboard Home Page key
            REGKEY("launchapplication1", Scancode::LaunchApplication1) //!< Keyboard Launch Application 1 key
            REGKEY("launchapplication2", Scancode::LaunchApplication2) //!< Keyboard Launch Application 2 key
            REGKEY("launchmail", Scancode::LaunchMail)         //!< Keyboard Launch Mail key
            REGKEY("launchmediaselect", Scancode::LaunchMediaSelect) //!< Keyboard Launch Media Select key
        lua_setfield(L, -2, "Key");

        lua_newtable(L);

            // Held
            lua_pushcfunction(L, [](lua_State* L) -> int {
                // Scancode arg
                int key = lua_tointeger(L, 1);
                sf::Keyboard::Scancode k = static_cast<sf::Keyboard::Scancode>(key);
                lua_pushboolean(L, Keys::get().held(k));
                return 1;
            });
            lua_setfield(L, -2, "check");

            // Press
            lua_pushcfunction(L, [](lua_State* L) -> int {
                // Scancode arg
                int key = lua_tointeger(L, 1);
                sf::Keyboard::Scancode k = static_cast<sf::Keyboard::Scancode>(key);
                lua_pushboolean(L, Keys::get().pressed(k));
                return 1;
            });
            lua_setfield(L, -2, "pressed");

            // Release
            lua_pushcfunction(L, [](lua_State* L) -> int {
                // Scancode arg
                int key = lua_tointeger(L, 1);
                sf::Keyboard::Scancode k = static_cast<sf::Keyboard::Scancode>(key);
                lua_pushboolean(L, Keys::get().released(k));
                return 1;
            });
            lua_setfield(L, -2, "released");

        lua_setfield(L, -2, "keyboard");

    lua_pop(L, 1);
}