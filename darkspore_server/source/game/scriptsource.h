
#ifndef _GAME_SCRIPTSOURCE_HEADER
#define _GAME_SCRIPTSOURCE_HEADER

// Include
#include <filesystem>
#include <string>

// Game
namespace Game {
	// ScriptSource
	// Reads the game's compiled Lua scripts. The game was built for 32 bit with float numbers,
	// so its bytecode has to be converted before this build of Lua can load it.
	class ScriptSource {
		public:
			static std::string Read(const std::filesystem::path& path);

			static bool IsGameBytecode(const std::string& data);
			static std::string ConvertBytecode(const std::string& data);
	};
}

#endif
