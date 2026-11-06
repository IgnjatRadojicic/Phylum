
#ifndef _GAME_MODIFIERCHECK_HEADER
#define _GAME_MODIFIERCHECK_HEADER

// Include
#include <filesystem>

// Game
namespace Game {
	class Instance;
	class Lua;

	// ModifierCheck
	// Runs the game's real modifier scripts on real objects, and writes what happened to a report.
	class ModifierCheck {
		public:
			static void Run(Instance& game, Lua& lua, const std::filesystem::path& reportPath);
	};
}

#endif
