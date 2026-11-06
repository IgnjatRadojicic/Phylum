
#ifndef _GAME_SCRIPTCHECK_HEADER
#define _GAME_SCRIPTCHECK_HEADER

// Include
#include <filesystem>

// Game
namespace Game {
	// ScriptCheck
	// Loads every game ability script without a game client, and writes what happened to a report.
	class ScriptCheck {
		public:
			static void WriteReport(const std::filesystem::path& reportPath);
	};
}

#endif
