
// Include
#include "scriptgaps.h"

// Game
namespace Game {
	// ScriptGaps
	ScriptGaps::ScriptGaps(std::ostream& log) : mLog(log) {}

	bool ScriptGaps::Record(const std::string& name) {
		if (!mSeen.insert(name).second) {
			return false;
		}

		mLog << name << std::endl;
		return true;
	}
}
