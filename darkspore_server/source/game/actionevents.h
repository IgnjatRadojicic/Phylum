
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
	// Tells running scripts what objects did besides hitting. Slots as the game's scripts read them
	// (see docs/events.md):
	//   UsedAbility: to the caster, Int 2 the ability's descriptors (modifier_stealth ends on it,
	//     the loot boosts and modifier_fireravager_passive check IsBasic).
	//   NearbyDeath: to everyone near a death, GUID 1 the one that died, Float 2 how far the
	//     witness was (modifier_arborealmight compares it with its own distance). The one that died
	//     hears it too: modifier_gravityorb_passive hides its orb when GUID 1 is its own agent.
	//   StartedNewAction: to an agent that starts a cast or is given somewhere to go. No slots.
	//     Sleeping Cloud's channel ends on it.
	//   BreakRoot: to an object that is teleported, before it moves (0xa09b2c). No slots. The
	//     roots end on it.
	//   LostModifierOfType: to an agent whose last modifier of a GUID ended, GUID 1 that modifier's
	//     GUID (0x9e1261). Stealth Attack ends when its stealth goes.
	//   PlayerSwap: to the hero that leaves the field. No slots. The Field Medic buff ends on it.
	//   Incorporeal: to a creature a projectile first goes through (see ProjectilePassList), GUID 1
	//     the creature, GUID 2 the projectile (0xa2f720). Ghostform's buff renews itself on it.
	class ActionEvents {
		public:
			explicit ActionEvents(AbilityEventSink& sink);

			void UsedAbility(uint32_t agent, uint32_t descriptors);
			void NearbyDeath(uint32_t dead, const std::vector<DeathWitness>& witnesses);
			void StartedNewAction(uint32_t agent);
			void BreakRoot(uint32_t object);
			void LostModifierOfType(uint32_t agent, uint32_t guid);
			void PlayerSwap(uint32_t outgoing);
			void Incorporeal(uint32_t target, uint32_t projectile);

		private:
			void SendPlain(uint32_t agent, AbilityEventType type);

		private:
			AbilityEventSink& mSink;
	};
}

#endif
