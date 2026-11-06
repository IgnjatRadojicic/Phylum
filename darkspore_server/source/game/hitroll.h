
#ifndef _GAME_HITROLL_HEADER
#define _GAME_HITROLL_HEADER

// Include
#include <cstdint>

// Game
namespace Game {
	// HitOutcome
	// What became of a hit before any damage, shown by the client as Dodge, Resist or Immune.
	enum class HitOutcome : uint8_t {
		Hit,
		Dodged,
		Resisted,
		Immune
	};

	// HitDefense
	// What the target brings against one hit.
	struct HitDefense {
		float physicalDefense = 0; // Rating that dodges physical hits
		float energyDefense = 0; // Rating that resists energy hits
		bool immune = false; // The Immune attribute
	};

	// HitRoll
	// Whether a hit lands, and how hard a critical one is. From the game's own scripts and data:
	//   The client's stat tooltip (modifier_binarysentinel_passive) shows dodge and resist percent
	//   as defense rating / DefenseRatingPerPercent, capped by DodgePercentCap and ResistPercentCap
	//   from MagicNumbers. Physical defense dodges, energy defense resists, and the client has
	//   Dodge and Resist combat flags for both, so either one stops the whole hit.
	//   A critical hit does CriticalDamageBonus (2 in MagicNumbers) times the damage.
	// Approximate: damage over time is never dodged or resisted, and CriticalDamageIncrease is
	// added to the bonus.
	class HitRoll {
		public:
			// The chance, from 0 to cap, that a rating avoids a hit.
			static float AvoidChance(float rating, float ratingPerPercent, float cap);

			// random is uniform in [0, 1).
			static HitOutcome Roll(bool physical, bool overTime, const HitDefense& defense,
				float ratingPerPercent, float dodgeCap, float resistCap, float random);

			// ratingPerPercent is 0 for a caster the rating table does not cover, which then
			// counts the rating in whole percents.
			static float CriticalChance(float criticalRating, float ratingPerPercent);
			static float CriticalMultiplier(float criticalDamageBonus, float criticalDamageIncrease);
	};
}

#endif
