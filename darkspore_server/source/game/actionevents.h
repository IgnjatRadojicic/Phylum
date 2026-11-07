
#ifndef _GAME_ACTIONEVENTS_HEADER
#define _GAME_ACTIONEVENTS_HEADER

// Include
#include "abilityevent.h"

#include <cstdint>
#include <vector>

// Game
namespace Game {
	// Someone near a death, and how far from it they were.
	struct DeathWitness {
		uint32_t id = 0;
		float distance = 0;
	};

	// ActionEvents
	// Tells running scripts what objects did besides hitting: UsedAbility to the caster's, and
	// NearbyDeath to everyone near a death. Slots as the game's scripts read them:
	//   UsedAbility: Int 2 the ability's descriptors (modifier_stealth ends on it, the loot
	//     boosts and modifier_fireravager_passive check IsBasic).
	//   NearbyDeath: GUID 1 the one that died, Float 2 how far the witness was
	//     (modifier_arborealmight compares it with its own distance). The one that died hears it
	//     too: modifier_gravityorb_passive hides its orb when GUID 1 is its own agent.
	class ActionEvents {
		public:
			explicit ActionEvents(AbilityEventSink& sink);

			void UsedAbility(uint32_t agent, uint32_t descriptors);
			void NearbyDeath(uint32_t dead, const std::vector<DeathWitness>& witnesses);

		private:
			AbilityEventSink& mSink;
	};
}

#endif
