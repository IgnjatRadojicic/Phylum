#include <doctest/doctest.h>

#include "game/projectilepasslist.h"

using namespace Game;

using Result = ProjectilePassList::Result;

TEST_CASE("a target with no incorporeal or intangible is hit") {
	ProjectilePassList list;
	CHECK(list.Check(7, 0.f, 0.f, 0.f) == Result::Hits);
}

TEST_CASE("incorporeal lets the shot pass when the roll is under it") {
	ProjectilePassList list;
	CHECK(list.Check(7, 0.5f, 0.f, 0.6f) == Result::Hits);
	CHECK(list.Check(7, 0.5f, 0.f, 0.4f) == Result::PassesFirstTime);
}

TEST_CASE("a target the shot passed once always lets it pass, without a new roll or event") {
	ProjectilePassList list;
	REQUIRE(list.Check(7, 1.f, 0.f, 0.f) == Result::PassesFirstTime);
	CHECK(list.Check(7, 0.f, 0.f, 0.99f) == Result::Passes);
}

TEST_CASE("intangible always lets the shot pass") {
	ProjectilePassList list;
	CHECK(list.Check(7, 0.f, 1.f, 0.99f) == Result::PassesFirstTime);
}

TEST_CASE("with the list full a passing target is not written down") {
	ProjectilePassList list;
	for (uint32_t id = 1; id <= ProjectilePassList::Capacity; ++id) {
		REQUIRE(list.Check(id, 1.f, 0.f, 0.f) == Result::PassesFirstTime);
	}

	CHECK(list.Check(100, 1.f, 0.f, 0.f) == Result::Passes);
	CHECK(list.Check(100, 0.f, 0.f, 0.f) == Result::Hits);
}
