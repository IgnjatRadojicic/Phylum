#include <doctest/doctest.h>

#include "game/attackerdamage.h"

using Game::AttackerBonuses;
using Game::AttackerDamage;
using Game::DamageSource;
using Game::DamageType;
using Game::Descriptors;

namespace {
	uint32_t Flags(std::initializer_list<Descriptors> flags) {
		uint32_t value = 0;
		for (const auto flag : flags) {
			value |= static_cast<uint32_t>(flag);
		}
		return value;
	}
}

TEST_CASE("the primary stat above the ignored amount scales by the coefficient") {
	// MagicNumbers PrimaryAttributeIgnoreAmount is 8
	CHECK(AttackerDamage::PrimaryScale(58, 8, 0.01f) == doctest::Approx(1.5f));
	CHECK(AttackerDamage::PrimaryScale(58, 8, 0) == doctest::Approx(1.f));
	CHECK(AttackerDamage::PrimaryScale(4, 8, 0.01f) == doctest::Approx(0.96f));
}

TEST_CASE("an attacker with no bonuses deals the damage unchanged") {
	const AttackerBonuses none;
	CHECK(AttackerDamage::Multiplier(none, Flags({ Descriptors::IsBasic, Descriptors::IsPhysicalDamage }), DamageType::Life, DamageSource::Physical) == doctest::Approx(1.f));
}

TEST_CASE("each matching bonus adds to the multiplier") {
	AttackerBonuses bonuses;
	bonuses.damageBuff = 0.1f;
	bonuses.typeDamage[static_cast<size_t>(DamageType::Spacetime)] = 0.2f;
	bonuses.projectileDamage = 0.05f;
	bonuses.energyDamageBuff = 0.03f;
	bonuses.areaDamage = 0.4f; // not an area hit
	bonuses.directAttackPercent = 0.07f;
	bonuses.damageOverTimeIncrease = 0.5f; // not over time
	bonuses.energyDamageDone = 0.02f;
	bonuses.energyDamageByAbility = 0.01f;

	const auto flags = Flags({ Descriptors::IsProjectile, Descriptors::IsEnergyDamage });
	CHECK(AttackerDamage::Multiplier(bonuses, flags, DamageType::Spacetime, DamageSource::Energy) == doctest::Approx(1.48f));
}

TEST_CASE("damage over time takes its own bonus instead of the direct one") {
	AttackerBonuses bonuses;
	bonuses.directAttackPercent = 0.07f;
	bonuses.damageOverTimeIncrease = 0.5f;
	bonuses.directAttackDamage = 12;

	const auto dot = Flags({ Descriptors::IsDoT });
	CHECK(AttackerDamage::Multiplier(bonuses, dot, DamageType::Generic, DamageSource::Physical) == doctest::Approx(1.5f));
	CHECK(AttackerDamage::DirectAttack(bonuses, dot) == 0.f);
	CHECK(AttackerDamage::DirectAttack(bonuses, Flags({ Descriptors::IsHoT })) == 0.f);
	CHECK(AttackerDamage::DirectAttack(bonuses, 0) == doctest::Approx(12.f));
}

TEST_CASE("a basic attack gains from defense and skips the ability-only bonuses") {
	AttackerBonuses bonuses;
	bonuses.physicalDefense = 100;
	bonuses.energyDefense = 50;
	bonuses.defenseBoostBasicDamage = 0.002f;
	bonuses.physicalDamageDone = 0.1f;
	bonuses.physicalDamageByAbility = 0.25f;

	const auto basic = Flags({ Descriptors::IsBasic, Descriptors::IsPhysicalDamage });
	CHECK(AttackerDamage::Multiplier(bonuses, basic, DamageType::Generic, DamageSource::Physical) == doctest::Approx(1.4f));

	const auto ability = Flags({ Descriptors::IsPhysicalDamage });
	CHECK(AttackerDamage::Multiplier(bonuses, ability, DamageType::Generic, DamageSource::Physical) == doctest::Approx(1.35f));
}
