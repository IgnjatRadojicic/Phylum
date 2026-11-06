
#ifndef _GAME_SCRIPTSOURCE_HEADER
#define _GAME_SCRIPTSOURCE_HEADER

// Include
#include <filesystem>
#include <string>

// Game
namespace Game {
	// ScriptSource
	// Reads the game's decompiled Lua scripts and repairs what the decompiler got wrong.
	class ScriptSource {
		public:
			static std::string Read(const std::filesystem::path& path);

			// The decompiler writes "L3_9 = L1_7(L2_8)" for a call that assigns L1, L2 and L3.
			static std::string RestoreMultipleAssignments(const std::string& source);
	};
}

#endif
