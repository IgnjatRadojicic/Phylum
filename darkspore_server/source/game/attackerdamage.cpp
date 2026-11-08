
// Include
#include "attackerdamage.h"

// Game
namespace Game {
	namespace {
		bool Has(uint32_t descriptors, Descriptors flag) {
			return (descriptors & static_cast<uint32_t>(flag)) != 0;
		}
	}

	float AttackerDamage::PrimaryScale(float primary, float ignoreAmount, float coefficient) {
		return 1.f + (primary - ignoreAmount) * coefficient;
	}

	float AttackerDamage::Multiplier(const AttackerBonuses& bonuses, uint32_t descriptors, DamageType type, DamageSource source) {
		const bool basic = Has(descriptors, Descriptors::IsBasic);

		float multiplier = 1.f;
		if (basic) {
			multiplier += (bonuses.physicalDefense + bonuses.energyDefense) * bonuses.defenseBoostBasicDamage;
		}
		multiplier += bonuses.damageBuff;

		// Generic damage has no type bonus (the client's switch covers the five types only).
		if (const auto index = static_cast<size_t>(type); index < bonuses.typeDamage.size()) {
			multiplier += bonuses.typeDamage[index];
		}
		if (Has(descriptors, Descriptors::IsProjectile)) {
			multiplier += bonuses.projectileDamage;
		}
		if (source == DamageSource::Energy) {
			multiplier += bonuses.energyDamageBuff;
		}
		if (Has(descriptors, Descriptors::IsAoE)) {
			multiplier += bonuses.areaDamage;
		}
		multiplier += Has(descriptors, Descriptors::IsDoT) ? bonuses.damageOverTimeIncrease : bonuses.directAttackPercent;

		if (Has(descriptors, Descriptors::IsPhysicalDamage)) {
			multiplier += bonuses.physicalDamageDone;
			if (!basic) {
				multiplier += bonuses.physicalDamageByAbility;
			}
		}
		if (Has(descriptors, Descriptors::IsEnergyDamage)) {
			multiplier += bonuses.energyDamageDone;
			if (!basic) {
				multiplier += bonuses.energyDamageByAbility;
			}
		}
		return multiplier;
	}

	float AttackerDamage::DirectAttack(const AttackerBonuses& bonuses, uint32_t descriptors) {
		if (Has(descriptors, Descriptors::IsDoT) || Has(descriptors, Descriptors::IsHoT)) {
			return 0.f;
		}
		return bonuses.directAttackDamage;
	}
}
