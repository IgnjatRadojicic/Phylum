#include <doctest/doctest.h>

#include "game/hitroll.h"
#include "game/magicnumbers.h"

#include <pugixml.hpp>

using Game::HitDefense;
using Game::HitOutcome;
using Game::HitRoll;

namespace {
	// Level 1-1 makes 10 rating one percent, and both caps are 80%.
	constexpr float perPercent = 10.f;
	constexpr float cap = 0.8f;

	HitOutcome Roll(bool physical, const HitDefense& defense, float random, bool overTime = false) {
		return HitRoll::Roll(physical, overTime, defense, perPercent, cap, cap, random);
	}
}

TEST_CASE("defense rating becomes a percent chance, as the client's tooltip shows it") {
	CHECK(HitRoll::AvoidChance(250, perPercent, cap) == doctest::Approx(0.25f));
	CHECK(HitRoll::AvoidChance(5000, perPercent, cap) == doctest::Approx(0.8f));
	CHECK(HitRoll::AvoidChance(-20, perPercent, cap) == 0.f);
	CHECK(HitRoll::AvoidChance(250, 0, cap) == 0.f);
}

TEST_CASE("physical defense dodges physical hits and energy defense resists energy ones") {
	HitDefense defense;
	defense.physicalDefense = 500; // 50%
	defense.energyDefense = 0;

	CHECK(Roll(true, defense, 0.49f) == HitOutcome::Dodged);
	CHECK(Roll(true, defense, 0.51f) == HitOutcome::Hit);
	CHECK(Roll(false, defense, 0.01f) == HitOutcome::Hit);

	defense.energyDefense = 300; // 30%
	CHECK(Roll(false, defense, 0.29f) == HitOutcome::Resisted);
}

TEST_CASE("immunity stops every hit, and damage over time is never avoided") {
	HitDefense defense;
	defense.physicalDefense = 5000;
	CHECK(Roll(true, defense, 0.f, true) == HitOutcome::Hit);

	defense.immune = true;
	CHECK(Roll(true, defense, 0.99f) == HitOutcome::Immune);
	CHECK(Roll(false, defense, 0.99f, true) == HitOutcome::Immune);
}

TEST_CASE("critical chance uses the rating table, or whole percents without it") {
	CHECK(HitRoll::CriticalChance(200, perPercent) == doctest::Approx(0.2f));
	CHECK(HitRoll::CriticalChance(20, 0) == doctest::Approx(0.2f));
	CHECK(HitRoll::CriticalChance(99999, perPercent) == 1.f);
}

TEST_CASE("a critical hit does the bonus from MagicNumbers plus any increase") {
	CHECK(HitRoll::CriticalMultiplier(2.f, 0.f) == 2.f);
	CHECK(HitRoll::CriticalMultiplier(2.f, 0.5f) == doctest::Approx(2.5f));
	CHECK(HitRoll::CriticalMultiplier(2.f, -5.f) == 1.f);
}

TEST_CASE("MagicNumbers reads the caps, the critical bonus and the per-point values") {
	pugi::xml_document document;
	document.load_string("<magicnumbers><DodgePercentCap>0.75</DodgePercentCap>"
		"<ResistPercentCap>0.6</ResistPercentCap><CriticalDamageBonus>3</CriticalDamageBonus>"
		"<HealthPerPointofStrength>4</HealthPerPointofStrength><ManaPerPointofMind>2</ManaPerPointofMind></magicnumbers>");
	const auto numbers = Game::MagicNumbers::FromXml(document.child("magicnumbers"));

	CHECK(numbers.dodgePercentCap == doctest::Approx(0.75f));
	CHECK(numbers.resistPercentCap == doctest::Approx(0.6f));
	CHECK(numbers.criticalDamageBonus == doctest::Approx(3.f));
	CHECK(numbers.healthPerStrength == doctest::Approx(4.f));
	CHECK(numbers.manaPerMind == doctest::Approx(2.f));
	CHECK(numbers.physicalDefensePerDexterity == doctest::Approx(6.f));

	const auto defaults = Game::MagicNumbers::FromXml(pugi::xml_node());
	CHECK(defaults.criticalDamageBonus == doctest::Approx(2.f));
}
