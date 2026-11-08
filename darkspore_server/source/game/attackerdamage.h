
#ifndef _GAME_ATTACKERDAMAGE_HEADER
#define _GAME_ATTACKERDAMAGE_HEADER

// Include
#include "attributes.h"

#include <array>
#include <cstdint>

// Game
namespace Game {
	// AttackerBonuses
	// The attacker's attributes that raise the damage it deals, read before a hit.
	struct AttackerBonuses {
		float damageBuff = 0; // DamageBuff
		std::array<float, 5> typeDamage {}; // TechnologyTypeDamage .. SupernaturalTypeDamage
		float projectileDamage = 0; // ProjectileDamageIncrease
		float energyDamageBuff = 0; // EnergyDamageBuff
		float areaDamage = 0; // AoEDamage
		float damageOverTimeIncrease = 0; // DoTDamageDoneIncrease
		float directAttackPercent = 0; // DirectAttackDamagePercent
		float physicalDamageDone = 0; // PhysicalDamageDoneIncrease
		float physicalDamageByAbility = 0; // PhysicalDamageDoneByAbilityIncrease
		float energyDamageDone = 0; // EnergyDamageDoneIncrease
		float energyDamageByAbility = 0; // EnergyDamageDoneByAbilityIncrease
		float physicalDefense = 0; // PhysicalDefense
		float energyDefense = 0; // EnergyDefense
		float defenseBoostBasicDamage = 0; // DefenseBoostBasicDamage
		float directAttackDamage = 0; // DirectAttackDamage
	};

	// AttackerDamage
	// What the attacker adds to an ability's damage, as the client computes it:
	//   PrimaryScale is 1 + 0x9e4e60: (primary stat - MagicNumbers.PrimaryAttributeIgnoreAmount)
	//   times the ability's damage coefficient.
	//   Multiplier is 0x9e4f40: 1, or for a basic attack 1 + (physical + energy defense) times
	//   DefenseBoostBasicDamage, plus each bonus that matches the hit.
	//   DirectAttack is 0x9e4ef0: DirectAttackDamage, except for damage or healing over time.
	class AttackerDamage {
		public:
			static float PrimaryScale(float primary, float ignoreAmount, float coefficient);
			static float Multiplier(const AttackerBonuses& bonuses, uint32_t descriptors, DamageType type, DamageSource source);
			static float DirectAttack(const AttackerBonuses& bonuses, uint32_t descriptors);
	};
}

#endif
