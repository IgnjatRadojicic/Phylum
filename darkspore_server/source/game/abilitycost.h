
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
		float cooldownVariance = 0; // A share of the cooldown it can randomly be longer or shorter by
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

	// CooldownRoll
	// The two random rolls behind cooldownVariance, each uniform in [0, 1).
	struct CooldownRoll {
		float amount = 0; // How much of the variance
		float direction = 0; // Below 0.5 makes the cooldown longer
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
	//   Cooldown: as the client works it out (Darkspore.exe 0x9dfc70, its ResetAbilityCooldown):
	//   the ranked cooldown, randomly longer or shorter by up to cooldownVariance of it. A basic
	//   attack's is then divided by attack speed + 1 (never below -0.9), and any other's is
	//   multiplied by 1 - CooldownScale.
	class AbilityCost {
		public:
			static CastCost For(const AbilityCostData& data, const CasterStats& caster, const CooldownRoll& roll = {});
	};
}

#endif
