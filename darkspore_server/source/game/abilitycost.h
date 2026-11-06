
#ifndef _GAME_ABILITYCOST_HEADER
#define _GAME_ABILITYCOST_HEADER

// Include
#include <cstdint>

// Game
namespace Game {
	// AbilityCostData
	// What an ability's script says a cast costs, already at the cast's rank.
	struct AbilityCostData {
		float manaCost = 0; // nAbility.GetManaCost from the game's global.lua
		float manaCoefficient = 0; // Extra mana per point of the ability's attribute
		float cooldownSeconds = 0;
		bool manual = false; // nCooldownType.Manual: the script pays with PayCooldownAndMana
		bool basic = false; // A basic attack, whose rate follows attack speed
	};

	// CasterStats
	// What the caster brings to the cost.
	struct CasterStats {
		float attributeValue = 0; // The ability's scaling attribute, as GetAbilityAttributeValue gives it
		float cooldownScale = 0; // CooldownScale. Overdrive adds 0.4 and calls it cooldownReduction.
		float attackSpeedScale = 0;
		bool overdrive = false; // Overdrive makes every cast free (its costReduction is 1)
	};

	// CastCost
	struct CastCost {
		float mana = 0;
		uint32_t cooldownMs = 0;
		bool manual = false;
	};

	// AbilityCost
	// Works out the mana and cooldown of one cast.
	//   Mana: the script's cost plus manaCoefficient times the attribute. ability_firetempest_active
	//   writes its own GetManaCost so that, with the coefficient part added on top, it comes out
	//   right, which shows the engine adds that part to every cost.
	//   Cooldown: shortened by CooldownScale as a fraction, which is approximate. A basic attack's
	//   cooldown also follows attack speed, the way its waits do.
	class AbilityCost {
		public:
			static CastCost For(const AbilityCostData& data, const CasterStats& caster);
	};
}

#endif
