
#include <doctest/doctest.h>

#include "game/projectilesweep.h"

using Game::ProjectileSweep;
using Game::SweepTarget;

namespace {
	constexpr uint32_t noTarget = 0;
	const glm::vec3 start(0, 0, 1);
	const glm::vec3 end(10, 0, 1);
	const glm::vec3 shotSize(0.5f, 0.5f, 0.5f);

	// A mob one unit wide each way and two tall, standing at a point on the ground.
	SweepTarget Mob(uint32_t id, float x, float y = 0, float z = 0) {
		return { id, { x, y, z }, { -1, -1, 0 }, { 1, 1, 2 } };
	}
}

TEST_CASE("a shot hits a mob on its path at the point of contact") {
	const auto hits = ProjectileSweep(start, end, shotSize, noTarget).Find({ Mob(7, 5) });

	REQUIRE(hits.size() == 1);
	CHECK(hits[0].id == 7);
	CHECK(hits[0].distance == doctest::Approx(3.5f));
	CHECK(hits[0].position.x == doctest::Approx(3.5f));
}

TEST_CASE("a fast shot cannot jump over a mob between ticks") {
	const auto hits = ProjectileSweep(start, { 1000, 0, 1 }, shotSize, noTarget).Find({ Mob(7, 500) });

	CHECK(hits.size() == 1);
}

TEST_CASE("a mob beside the path or past its end is not hit") {
	const ProjectileSweep sweep(start, end, shotSize, noTarget);

	CHECK(sweep.Find({ Mob(1, 5, 3) }).empty());
	CHECK(sweep.Find({ Mob(2, 20) }).empty());
	CHECK(sweep.Find({ Mob(3, -5) }).empty());
}

TEST_CASE("a shot passes over a mob it is too high to touch") {
	const auto hits = ProjectileSweep({ 0, 0, 5 }, { 10, 0, 5 }, shotSize, noTarget).Find({ Mob(7, 5) });

	CHECK(hits.empty());
}

TEST_CASE("a shot hits a flying mob at its height") {
	const auto hits = ProjectileSweep({ 0, 0, 6 }, { 10, 0, 6 }, shotSize, noTarget).Find({ Mob(7, 5, 0, 5) });

	CHECK(hits.size() == 1);
}

TEST_CASE("a tall shot reaches a mob that a short one passes over") {
	const glm::vec3 tall(0.5f, 0.5f, 3.5f);
	const auto hits = ProjectileSweep({ 0, 0, 5 }, { 10, 0, 5 }, tall, noTarget).Find({ Mob(7, 5) });

	CHECK(hits.size() == 1);
}

TEST_CASE("a falling shot hits on the way down") {
	const auto hits = ProjectileSweep({ 5, 0, 10 }, { 5, 0, 0 }, shotSize, noTarget).Find({ Mob(7, 5) });

	REQUIRE(hits.size() == 1);
	CHECK(hits[0].position.z == doctest::Approx(2.5f));
}

TEST_CASE("a diagonal shot clips the corner of a box") {
	const auto hits = ProjectileSweep({ 0, 2, 1 }, { 4, -2, 1 }, shotSize, noTarget).Find({ Mob(7, 3, 0) });

	CHECK(hits.size() == 1);
}

TEST_CASE("a mob already touching the shot is hit at once") {
	const auto hits = ProjectileSweep(start, end, shotSize, noTarget).Find({ Mob(7, 0.5f) });

	REQUIRE(hits.size() == 1);
	CHECK(hits[0].distance == 0);
}

TEST_CASE("a shot that has not moved still hits what it touches") {
	const ProjectileSweep sweep(start, start, shotSize, noTarget);

	CHECK(sweep.Find({ Mob(7, 1) }).size() == 1);
	CHECK(sweep.Find({ Mob(8, 5) }).empty());
}

TEST_CASE("hits come back nearest first whatever order the mobs are listed") {
	const auto hits = ProjectileSweep(start, end, shotSize, noTarget).Find({ Mob(1, 8), Mob(2, 2), Mob(3, 5) });

	REQUIRE(hits.size() == 3);
	CHECK(hits[0].id == 2);
	CHECK(hits[1].id == 3);
	CHECK(hits[2].id == 1);
}

TEST_CASE("mobs stacked on one point are ordered by lowest id") {
	const auto hits = ProjectileSweep(start, end, shotSize, noTarget).Find({ Mob(30, 5), Mob(10, 5), Mob(20, 5) });

	REQUIRE(hits.size() == 3);
	CHECK(hits[0].id == 10);
	CHECK(hits[1].id == 20);
	CHECK(hits[2].id == 30);
}

TEST_CASE("the intended target wins a tie in a stack") {
	const auto hits = ProjectileSweep(start, end, shotSize, 30).Find({ Mob(10, 5), Mob(30, 5), Mob(20, 5) });

	REQUIRE(hits.size() == 3);
	CHECK(hits[0].id == 30);
	CHECK(hits[1].id == 10);
}

TEST_CASE("the intended target does not win over a mob clearly in front of it") {
	const auto hits = ProjectileSweep(start, end, shotSize, 30).Find({ Mob(30, 8), Mob(10, 2) });

	REQUIRE(hits.size() == 2);
	CHECK(hits[0].id == 10);
}
