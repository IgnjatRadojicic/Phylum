#include <doctest/doctest.h>

#include "game/triggeroccupancy.h"
#include "game/triggerarea.h"

#include <vector>

using Game::TriggerOccupancy;
using Game::TriggerArea;

TEST_CASE("an object enters once, stays while inside and exits once") {
	TriggerOccupancy occupancy;

	auto events = occupancy.Update({ 5, 7, 5 });
	CHECK(events.entered == std::vector<uint32_t> { 5, 7 });
	CHECK(events.stayed.empty());
	CHECK(events.exited.empty());

	events = occupancy.Update({ 7 });
	CHECK(events.entered.empty());
	CHECK(events.stayed == std::vector<uint32_t> { 7 });
	CHECK(events.exited == std::vector<uint32_t> { 5 });

	events = occupancy.Update({});
	CHECK(events.exited == std::vector<uint32_t> { 7 });
	CHECK_FALSE(occupancy.Contains(7));
}

TEST_CASE("an object that leaves the world exits only when it was inside") {
	TriggerOccupancy occupancy;
	occupancy.Update({ 3 });

	CHECK(occupancy.Remove(3));
	CHECK_FALSE(occupancy.Remove(3));
	CHECK_FALSE(occupancy.Remove(9));
	CHECK(occupancy.Update({}).exited.empty());
}

TEST_CASE("a sphere counts a body that touches it") {
	auto sphere = TriggerArea::Sphere({ 0, 0, 0 }, 5);
	CHECK(sphere.Touches({ 4, 0, 0 }, 0));
	CHECK(sphere.Touches({ 6, 0, 0 }, 1.5f));
	CHECK_FALSE(sphere.Touches({ 6, 0, 0 }, 0.5f));
	CHECK(sphere.GetReach() == doctest::Approx(5));

	sphere.MoveTo({ 10, 0, 0 });
	CHECK(sphere.Touches({ 14, 0, 0 }, 0));
	CHECK_FALSE(sphere.Touches({ 4, 0, 0 }, 0));
}

TEST_CASE("a box turns to its facing, and a thin wall still meets a body") {
	// A wall 0.1 wide and 4 long, facing +X: it runs along X.
	const auto wall = TriggerArea::Box({ 0, 0, 0 }, { 0.1f, 4, 0.1f }, { 3, 0, 0 });
	CHECK(wall.Touches({ 3.9f, 0, 0 }, 0));
	CHECK_FALSE(wall.Touches({ 0, 1, 0 }, 0));
	CHECK(wall.Touches({ 0, 1, 0 }, 1));
	CHECK_FALSE(wall.Touches({ 5, 0, 0 }, 0.5f));

	// No facing on the ground faces +Y.
	const auto upright = TriggerArea::Box({ 0, 0, 0 }, { 1, 3, 1 }, { 0, 0, 1 });
	CHECK(upright.Touches({ 0, 2.5f, 0 }, 0));
	CHECK_FALSE(upright.Touches({ 2.5f, 0, 0 }, 0));
}
