
#ifndef _GAME_DAMAGEEVENTS_HEADER
#define _GAME_DAMAGEEVENTS_HEADER

// Include
#include "abilityevent.h"

#include <cstdint>

// Game
namespace Game {
	// A hit that landed, as the damage events describe it. Zero means unknown.
	struct DamageReport {
		uint32_t victim = 0;
		uint32_t attacker = 0;
		float amount = 0.f;
		uint32_t descriptors = 0;
		uint32_t damageType = 0;
		uint32_t damageSource = 0;
		uint32_t sourceModifier = 0;
		uint32_t ability = 0;
	};

	// DamageEvents
	// Tells running scripts about a hit: TookDamage to the victim's, DealtDamage to the attacker's.
	//
	// The slots are inferred from how the game's scripts read them, and are approximate
	// (see docs/modifiers.md): GUID 1 the other side, Float 2 the amount, Int 3 the descriptors,
	// Int 4 the damage type, Int 5 the damage source, GUID 6 the modifier that did it and
	// GUID 7 the ability.
	//
	// A hit dealt while these events are being handled raises none of its own. Otherwise a
	// reflected hit would bounce between two thorns effects forever. The original game's
	// rule for this is unknown.
	class DamageEvents {
		public:
			explicit DamageEvents(AbilityEventSink& sink);

			void Report(const DamageReport& hit);

		private:
			AbilityEvent Describe(AbilityEventType type, uint32_t otherSide, const DamageReport& hit) const;

		private:
			AbilityEventSink& mSink;
			bool mSending = false;
	};
}

#endif
