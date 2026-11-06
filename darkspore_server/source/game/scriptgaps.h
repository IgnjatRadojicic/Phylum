
#ifndef _GAME_SCRIPTGAPS_HEADER
#define _GAME_SCRIPTGAPS_HEADER

// Include
#include <ostream>
#include <string>
#include <unordered_set>

// Game
namespace Game {
	// ScriptGaps
	// Remembers what game scripts asked for that the server does not have. Each name is
	// written once and flushed at once, so the record survives a crash.
	class ScriptGaps {
		public:
			explicit ScriptGaps(std::ostream& log);

			// Returns true the first time a name is seen.
			bool Record(const std::string& name);

		private:
			std::ostream& mLog;
			std::unordered_set<std::string> mSeen;
	};
}

#endif
