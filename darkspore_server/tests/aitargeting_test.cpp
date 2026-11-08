
#include <doctest/doctest.h>

#include "game/aitargeting.h"

using Game::OrderHostiles;
using Game::PickNearestHostile;
using Game::TargetCandidate;

namespace {
	constexpr uint8_t heroTeam = 1;
	constexpr uint8_t mobTeam = 0;
	constexpr uint32_t turret = 100;
	const glm::vec3 here(0, 0, 0);

	TargetCandidate Creature(uint32_t id, float x, uint8_t team = mobTeam, bool alive = true) {
		return { id, { x, 0, 0 }, team, alive };
	}
}

TEST_CASE("the nearest creature on another team is the target") {
	const std::vector candidates { Creature(1, 9), Creature(2, 4), Creature(3, 6) };
	CHECK(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 2);
}

TEST_CASE("a creature on its own team is never the target, however near") {
	const std::vector candidates { Creature(1, 1, heroTeam), Creature(2, 8) };
	CHECK(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 2);
	// A mob's team has the heroes as its hostiles.
	CHECK(PickNearestHostile(here, mobTeam, 12, candidates, turret) == 1);
}

TEST_CASE("the AI never picks itself") {
	const std::vector candidates { Creature(turret, 0), Creature(2, 5) };
	CHECK(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 2);
}

TEST_CASE("nothing beyond the radius is a target") {
	CHECK(PickNearestHostile(here, heroTeam, 12, { Creature(1, 13) }, turret) == 0);
	CHECK(PickNearestHostile(here, heroTeam, 12, { Creature(1, 12) }, turret) == 1);
	CHECK(PickNearestHostile(here, heroTeam, 0, { Creature(1, 1) }, turret) == 0);
}

TEST_CASE("a tie goes to the lower id, whatever the order") {
	const std::vector candidates { Creature(30, 5), Creature(10, 5), Creature(20, 5) };
	CHECK(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 10);
}

TEST_CASE("a target that dies is dropped for the next nearest") {
	std::vector candidates { Creature(1, 3), Creature(2, 7) };
	REQUIRE(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 1);

	candidates[0].alive = false;
	CHECK(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 2);
}

TEST_CASE("a target that is removed is dropped, and the AI waits with none") {
	std::vector candidates { Creature(1, 3) };
	REQUIRE(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 1);

	candidates.clear();
	CHECK(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 0);
}

TEST_CASE("a new hostile in range is acquired after the last one went") {
	std::vector candidates { Creature(1, 3, mobTeam, false) };
	REQUIRE(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 0);

	candidates.push_back(Creature(7, 10));
	CHECK(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 7);

	// One that walks nearer than the current target takes over.
	candidates.push_back(Creature(8, 2));
	CHECK(PickNearestHostile(here, heroTeam, 12, candidates, turret) == 8);
}

TEST_CASE("an untargetable creature is never a target") {
	auto hidden = Creature(1, 2);
	hidden.targetable = false;
	CHECK(PickNearestHostile(here, heroTeam, 12, { hidden, Creature(2, 6) }, turret) == 2);
}

TEST_CASE("a stealthed creature is a target only for an agent that detects stealth") {
	auto sneaking = Creature(1, 2);
	sneaking.stealthed = true;
	const std::vector candidates { sneaking, Creature(2, 6) };
	CHECK(OrderHostiles(here, heroTeam, 12, false, candidates, turret) == std::vector<uint32_t> { 2 });
	CHECK(OrderHostiles(here, heroTeam, 12, true, candidates, turret) == std::vector<uint32_t> { 1, 2 });
}

TEST_CASE("the aggro list is every hostile in range, nearest first") {
	const std::vector candidates { Creature(5, 9), Creature(3, 4), Creature(4, 4), Creature(6, 1, heroTeam), Creature(7, 20) };
	CHECK(OrderHostiles(here, heroTeam, 12, false, candidates, turret) == std::vector<uint32_t> { 3, 4, 5 });
}
