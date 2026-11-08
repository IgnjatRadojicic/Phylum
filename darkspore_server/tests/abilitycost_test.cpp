#include <doctest/doctest.h>

#include "game/abilitycost.h"

using Game::AbilityCost;
using Game::AbilityCostData;
using Game::CasterStats;

TEST_CASE("mana is the script's cost plus the coefficient times the attribute") {
	// nAbility_QuantumState at rank 1: manaCost 16, manaCoefficient 0.08
	const auto cost = AbilityCost::For({ 16, 0.08f, 20 }, { 100 });

	CHECK(cost.mana == doctest::Approx(24.f));
	CHECK(cost.cooldownMs == 20000);
	CHECK_FALSE(cost.manual);
}

TEST_CASE("a free ability stays free whatever its coefficient") {
	const auto cost = AbilityCost::For({ 0, 0.08f, 5 }, { 100 });

	CHECK(cost.mana == 0.f);
	CHECK(cost.cooldownMs == 5000);
}

TEST_CASE("a cast in overdrive costs no mana but still cools down") {
	CasterStats caster { 100 };
	caster.overdrive = true;
	caster.cooldownScale = 0.4f;
	const auto cost = AbilityCost::For({ 16, 0.08f, 10 }, caster);

	CHECK(cost.mana == 0.f);
	CHECK(cost.cooldownMs == 6000);
}

TEST_CASE("cooldown scale can shorten a cooldown to nothing, or lengthen it") {
	CasterStats faster;
	faster.cooldownScale = 2.f;
	CHECK(AbilityCost::For({ 0, 0, 5 }, faster).cooldownMs == 0);

	CasterStats slower;
	slower.cooldownScale = -1.f;
	CHECK(AbilityCost::For({ 0, 0, 5 }, slower).cooldownMs == 10000);
}

TEST_CASE("a basic attack's cooldown follows attack speed") {
	AbilityCostData basic { 0, 0, 0.7f };
	basic.basic = true;
	CasterStats caster;
	caster.attackSpeedScale = 0.4f;

	CHECK(AbilityCost::For(basic, caster).cooldownMs == 500);
}

TEST_CASE("a manual ability says so, and a negative cost is free") {
	AbilityCostData manual { -5, 0, 3 };
	manual.manual = true;
	const auto cost = AbilityCost::For(manual, {});

	CHECK(cost.manual);
	CHECK(cost.mana == 0.f);
	CHECK(cost.cooldownMs == 3000);
}

TEST_CASE("a basic attack's cooldown ignores cooldown scale") {
	AbilityCostData basic { 0, 0, 1 };
	basic.basic = true;
	CasterStats caster;
	caster.cooldownScale = 0.5f;
	CHECK(AbilityCost::For(basic, caster).cooldownMs == 1000);
}

TEST_CASE("cooldown variance makes a cooldown randomly longer or shorter") {
	AbilityCostData data { 0, 0, 4 };
	data.cooldownVariance = 0.5f;
	CHECK(AbilityCost::For(data, {}, { 1.f, 0.f }).cooldownMs == 6000);
	CHECK(AbilityCost::For(data, {}, { 0.5f, 0.9f }).cooldownMs == 3000);
	CHECK(AbilityCost::For(data, {}).cooldownMs == 4000);
}
