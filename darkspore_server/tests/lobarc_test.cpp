
#include <doctest/doctest.h>

#include "game/lobarc.h"

using Game::LobArc;

namespace {
	const glm::vec3 start(0, 0, 0);

	float GetHighestPoint(const LobArc& arc) {
		float highest = -1000;
		for (int step = 0; step <= 1000; ++step) {
			highest = std::max(highest, arc.GetPositionAt(arc.mDuration * step / 1000.f).z);
		}
		return highest;
	}
}

TEST_CASE("a lob starts at the start and ends at the destination") {
	const auto arc = LobArc::Build(start, { 10, 0, 0 }, 4, 1, 0);

	REQUIRE(arc);
	CHECK(arc->GetPositionAt(0).x == doctest::Approx(0));
	CHECK(arc->GetPositionAt(0).z == doctest::Approx(0));
	CHECK(arc->GetPositionAt(1).x == doctest::Approx(10));
	CHECK(arc->GetPositionAt(1).z == doctest::Approx(0));
}

TEST_CASE("a lob on level ground peaks at its height halfway along") {
	const auto arc = LobArc::Build(start, { 10, 0, 0 }, 4, 1, 0);

	REQUIRE(arc);
	CHECK(arc->GetPositionAt(0.5f).x == doctest::Approx(5));
	CHECK(arc->GetPositionAt(0.5f).z == doctest::Approx(4));
}

TEST_CASE("the curve just before landing runs into the destination") {
	const auto arc = LobArc::Build(start, { 10, 0, 2 }, 4, 1, 0);

	REQUIRE(arc);
	CHECK(arc->GetPositionAt(0.9999f).z == doctest::Approx(2).epsilon(0.01));
}

TEST_CASE("a lob uphill clears the destination by its height") {
	const auto arc = LobArc::Build(start, { 10, 0, 3 }, 4, 1, 0);

	REQUIRE(arc);
	CHECK(GetHighestPoint(*arc) == doctest::Approx(7).epsilon(0.01));
}

TEST_CASE("a lob downhill rises by its height above the start") {
	const auto arc = LobArc::Build({ 0, 0, 5 }, { 10, 0, 0 }, 4, 1, 0);

	REQUIRE(arc);
	CHECK(GetHighestPoint(*arc) == doctest::Approx(9).epsilon(0.01));
	CHECK(arc->GetPositionAt(0.9999f).z == doctest::Approx(0).epsilon(0.02));
}

TEST_CASE("with no height a downhill lob only falls") {
	const auto arc = LobArc::Build({ 0, 0, 5 }, { 10, 0, 0 }, 0, 1, 0);

	REQUIRE(arc);
	CHECK(GetHighestPoint(*arc) == doctest::Approx(5));
	CHECK(arc->GetPositionAt(0.9999f).z == doctest::Approx(0).epsilon(0.02));
}

TEST_CASE("flight time wins over speed") {
	const auto arc = LobArc::Build(start, { 10, 0, 0 }, 4, 2, 100);

	REQUIRE(arc);
	CHECK(arc->mDuration == doctest::Approx(2));
}

TEST_CASE("speed sets the flight time when none is given") {
	const auto arc = LobArc::Build(start, { 10, 0, 0 }, 4, 0, 5);

	REQUIRE(arc);
	CHECK(arc->mDuration == doctest::Approx(2));
}

TEST_CASE("a lob works in any direction across the ground") {
	const auto arc = LobArc::Build({ 3, 4, 0 }, { 0, 0, 0 }, 2, 1, 0);

	REQUIRE(arc);
	CHECK(arc->GetPositionAt(0.5f).x == doctest::Approx(1.5f));
	CHECK(arc->GetPositionAt(0.5f).y == doctest::Approx(2));
}

TEST_CASE("the launch velocity matches the ground speed") {
	const auto arc = LobArc::Build(start, { 0, 10, 0 }, 4, 2, 0);

	REQUIRE(arc);
	CHECK(arc->GetInitialVelocity().y == doctest::Approx(5));
	CHECK(arc->GetInitialVelocity().z == doctest::Approx(arc->mUpLinear));
}

TEST_CASE("a lob with no ground to cover or no time gives nothing") {
	CHECK_FALSE(LobArc::Build(start, { 0, 0, 5 }, 4, 1, 0));
	CHECK_FALSE(LobArc::Build(start, { 10, 0, 0 }, 4, 0, 0));
	CHECK_FALSE(LobArc::Build(start, { 10, 0, 0 }, -1, 1, 0));
}
