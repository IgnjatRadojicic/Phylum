
#include <doctest/doctest.h>

#include "game/projectilesweep.h"

using Game::ProjectileSweep;
using Game::SweepTarget;

namespace {
	constexpr uint32_t noTarget = 0;
	const glm::vec3 start(0, 0, 0);
	const glm::vec3 end(10, 0, 0);
}

TEST_CASE("a shot hits a mob on its path at the point of contact") {
	const auto hits = ProjectileSweep(start, end, 0.5f, noTarget).Find({ { 7, { 5, 0, 0 }, 1 } });

	REQUIRE(hits.size() == 1);
	CHECK(hits[0].id == 7);
	CHECK(hits[0].distance == doctest::Approx(3.5f));
	CHECK(hits[0].position.x == doctest::Approx(3.5f));
}

TEST_CASE("a fast shot cannot jump over a mob between ticks") {
	const auto hits = ProjectileSweep(start, { 1000, 0, 0 }, 0.1f, noTarget).Find({ { 7, { 500, 0, 0 }, 0.5f } });

	CHECK(hits.size() == 1);
}

TEST_CASE("a mob beside the path or past its end is not hit") {
	const ProjectileSweep sweep(start, end, 0.5f, noTarget);

	CHECK(sweep.Find({ { 1, { 5, 3, 0 }, 1 } }).empty());
	CHECK(sweep.Find({ { 2, { 20, 0, 0 }, 1 } }).empty());
	CHECK(sweep.Find({ { 3, { -5, 0, 0 }, 1 } }).empty());
}

TEST_CASE("a mob already touching the shot is hit at once") {
	const auto hits = ProjectileSweep(start, end, 0.5f, noTarget).Find({ { 7, { 0.5f, 0, 0 }, 1 } });

	REQUIRE(hits.size() == 1);
	CHECK(hits[0].distance == 0);
}

TEST_CASE("a shot that has not moved still hits what it touches") {
	const ProjectileSweep sweep(start, start, 0.5f, noTarget);

	CHECK(sweep.Find({ { 7, { 1, 0, 0 }, 1 } }).size() == 1);
	CHECK(sweep.Find({ { 8, { 5, 0, 0 }, 1 } }).empty());
}

TEST_CASE("height does not make a shot miss a mob it crosses") {
	const auto hits = ProjectileSweep({ 0, 0, 3 }, { 10, 0, 3 }, 0.5f, noTarget).Find({ { 7, { 5, 0, 0 }, 1 } });

	CHECK(hits.size() == 1);
}

TEST_CASE("hits come back nearest first whatever order the mobs are listed") {
	const auto hits = ProjectileSweep(start, end, 0.5f, noTarget).Find({
		{ 1, { 8, 0, 0 }, 1 }, { 2, { 2, 0, 0 }, 1 }, { 3, { 5, 0, 0 }, 1 }
	});

	REQUIRE(hits.size() == 3);
	CHECK(hits[0].id == 2);
	CHECK(hits[1].id == 3);
	CHECK(hits[2].id == 1);
}

TEST_CASE("mobs stacked on one point are ordered by lowest id") {
	const glm::vec3 point(5, 0, 0);
	const auto hits = ProjectileSweep(start, end, 0.5f, noTarget).Find({
		{ 30, point, 1 }, { 10, point, 1 }, { 20, point, 1 }
	});

	REQUIRE(hits.size() == 3);
	CHECK(hits[0].id == 10);
	CHECK(hits[1].id == 20);
	CHECK(hits[2].id == 30);
}

TEST_CASE("the intended target wins a tie in a stack") {
	const glm::vec3 point(5, 0, 0);
	const auto hits = ProjectileSweep(start, end, 0.5f, 30).Find({
		{ 10, point, 1 }, { 30, point, 1 }, { 20, point, 1 }
	});

	REQUIRE(hits.size() == 3);
	CHECK(hits[0].id == 30);
	CHECK(hits[1].id == 10);
}

TEST_CASE("the intended target does not win over a mob clearly in front of it") {
	const auto hits = ProjectileSweep(start, end, 0.5f, 30).Find({
		{ 30, { 8, 0, 0 }, 1 }, { 10, { 2, 0, 0 }, 1 }
	});

	REQUIRE(hits.size() == 2);
	CHECK(hits[0].id == 10);
}
