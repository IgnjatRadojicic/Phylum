
#ifndef _GAME_MAGICNUMBERS_HEADER
#define _GAME_MAGICNUMBERS_HEADER

// Include
#include <cstdint>

namespace pugi {
	class xml_node;
}

// Game
namespace Game {
	// MagicNumbers
	// The game's combat tuning sheet (MagicNumbers.MagicNumbers.xml). Only the values the server
	// uses are read. The defaults are the file's own values, for when it cannot be read.
	struct MagicNumbers {
		float dodgePercentCap = 0.8f; // nTuning.DodgePercentCap
		float resistPercentCap = 0.8f; // nTuning.ResistPercentCap
		float criticalDamageBonus = 2.f; // A critical hit's damage multiplier

		// Reads the <magicnumbers> root of the game's file.
		static MagicNumbers FromXml(const pugi::xml_node& root);

		// data/serverdata/magicnumbers, read on first use.
		static const MagicNumbers& Game();
	};
}

#endif
