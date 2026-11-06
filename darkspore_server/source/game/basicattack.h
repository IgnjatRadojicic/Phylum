
#ifndef _GAME_BASICATTACK_HEADER
#define _GAME_BASICATTACK_HEADER

// Include
#include <cstdint>

// Game
namespace Game {
	// BasicAttackStep
	struct BasicAttackStep {
		uint32_t animation = 0;
		uint32_t hitDelay = 0; // milliseconds from the start of the animation to the hit
	};

	// BasicAttacks
	// The game's ability scripts cannot run on the server yet, so this reads only the
	// animation sequence of basic attacks from them.
	class BasicAttacks {
		public:
			// Returns the next swing of the attack for this object, or nullptr if the ability has none.
			static const BasicAttackStep* NextStep(uint32_t abilityId, uint32_t objectId);
	};
}

#endif
