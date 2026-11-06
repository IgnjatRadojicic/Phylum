#include <doctest/doctest.h>

#include "game/magicnumbers.h"
#include "game/statshares.h"

using Game::AttributeType;
using Game::MagicNumbers;
using Game::StatShares;

TEST_CASE("each derived stat grows with its primary stat, as the client builds it") {
	const StatShares shares { MagicNumbers() };

	const auto health = shares.For(AttributeType::MaxHealth);
	REQUIRE(health);
	CHECK(health->primary == AttributeType::Strength);
	CHECK(StatShares::Value(*health, 20, 10) == doctest::Approx(70.f));

	const auto defense = shares.For(AttributeType::PhysicalDefense);
	REQUIRE(defense);
	CHECK(defense->primary == AttributeType::Dexterity);
	CHECK(StatShares::Value(*defense, 0, 10) == doctest::Approx(60.f));

	CHECK(shares.For(AttributeType::EnergyDefense)->primary == AttributeType::Mind);
	CHECK(shares.For(AttributeType::CriticalRating)->perPoint == doctest::Approx(4.f));
	CHECK(StatShares::Value(*shares.For(AttributeType::MaxMana), 75, 10) == doctest::Approx(85.f));
}

TEST_CASE("only the three primary stats feed others") {
	const StatShares shares { MagicNumbers() };
	CHECK(shares.IsPrimary(AttributeType::Strength));
	CHECK(shares.IsPrimary(AttributeType::Dexterity));
	CHECK(shares.IsPrimary(AttributeType::Mind));
	CHECK_FALSE(shares.IsPrimary(AttributeType::MaxHealth));
	CHECK_FALSE(shares.For(AttributeType::MovementSpeedBuff));
}

TEST_CASE("the per-point values come from MagicNumbers") {
	MagicNumbers numbers;
	numbers.healthPerStrength = 7;
	const StatShares shares { numbers };
	CHECK(StatShares::Value(*shares.For(AttributeType::MaxHealth), 0, 10) == doctest::Approx(70.f));
}
