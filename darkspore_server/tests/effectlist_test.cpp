#include <doctest/doctest.h>

#include "game/effectlist.h"

using Game::EffectList;

TEST_CASE("each copy of one effect gets its own index, removed newest first") {
	EffectList effects;
	const auto first = effects.Add(7);
	const auto second = effects.Add(7);
	const auto third = effects.Add(7);
	CHECK(first != second);
	CHECK(second != third);
	CHECK(effects.Remove(7) == third);
	CHECK(effects.Remove(7) == second);
	CHECK(effects.Remove(7) == first);
	CHECK(effects.Remove(7) == 0);
}

TEST_CASE("removing an effect never touches another effect's index") {
	EffectList effects;
	const auto rocks = effects.Add(1);
	const auto shield = effects.Add(2);
	CHECK(effects.Remove(2) == shield);
	CHECK(effects.Remove(1) == rocks);
}

TEST_CASE("a freed index is reused, and removing by index drops that copy") {
	EffectList effects;
	const auto first = effects.Add(7);
	effects.Add(7);
	CHECK(effects.RemoveByIndex(first));
	CHECK_FALSE(effects.RemoveByIndex(first));
	CHECK(effects.Add(9) == first);
	CHECK(effects.Remove(7) != 0);
	CHECK(effects.Remove(7) == 0);
}
