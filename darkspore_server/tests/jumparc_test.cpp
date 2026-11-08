#include <doctest/doctest.h>

#include "game/jumparc.h"

#include <limits>

using Game::JumpArc;

TEST_CASE("a jump covers its distance at its speed and lands on the ground") {
	const auto arc = JumpArc::Build({ 0, 0, 0 }, { 2, 0, 0 }, 10, 20, 1, 3, 20);
	REQUIRE(arc);
	CHECK(arc->GetDuration() == doctest::Approx(0.5f));
	CHECK(arc->GetDestination().x == doctest::Approx(10));
	CHECK(arc->GetPositionAt(0.25f).x == doctest::Approx(5));
	CHECK(arc->GetPositionAt(5).z == doctest::Approx(0));
	CHECK_FALSE(arc->IsFinished(0.4f));
	CHECK(arc->IsFinished(0.5f));
}

TEST_CASE("a jump to a position that is not a number never starts") {
	// Quantum Blink cast at the hero's own spot slid to inf/NaN, and the client crashed on it.
	const float inf = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	CHECK_FALSE(JumpArc::Build({ 0, 0, 0 }, { inf, inf, 0 }, inf, 100, 0, 0, 0));
	CHECK_FALSE(JumpArc::Build({ 0, 0, 0 }, { nan, nan, nan }, nan, 100, 0, 0, 0));
	CHECK_FALSE(JumpArc::Build({ nan, 0, 0 }, { 1, 0, 0 }, 5, 100, 0, 0, 0));
	CHECK_FALSE(JumpArc::Build({ 0, 0, 0 }, { 1, 0, 0 }, 5, nan, 0, 0, 0));
}

TEST_CASE("a jump's height grows with its distance up to the maximum") {
	// Half the range to the maximum gives half way from the minimum to the maximum.
	const auto halfway = JumpArc::Build({ 0, 0, 0 }, { 1, 0, 0 }, 10, 10, 1, 3, 20);
	REQUIRE(halfway);
	CHECK(halfway->GetHeight() == doctest::Approx(2));
	CHECK(halfway->GetPositionAt(0.5f).z == doctest::Approx(2));

	const auto far = JumpArc::Build({ 0, 0, 0 }, { 1, 0, 0 }, 40, 10, 1, 3, 20);
	CHECK(far->GetHeight() == doctest::Approx(3));

	// A slide has no height.
	const auto slide = JumpArc::Build({ 0, 0, 0 }, { 0, 1, 0 }, 5, 100, 0, 0, 0);
	CHECK(slide->GetPositionAt(0.025f).z == doctest::Approx(0));
}

TEST_CASE("a jump cut short comes down below where it was") {
	const auto arc = JumpArc::Build({ 0, 0, 0 }, { 1, 0, 0 }, 10, 10, 1, 3, 20);
	REQUIRE(arc);
	CHECK(arc->GetPositionAt(0.5f).z > 0);
	CHECK(arc->GetGroundAt(0.5f).x == doctest::Approx(5));
	CHECK(arc->GetGroundAt(0.5f).z == doctest::Approx(0));
}

TEST_CASE("a jump with no distance, speed or direction does not start") {
	CHECK_FALSE(JumpArc::Build({ 0, 0, 0 }, { 1, 0, 0 }, 0, 10, 0, 0, 0));
	CHECK_FALSE(JumpArc::Build({ 0, 0, 0 }, { 1, 0, 0 }, 5, 0, 0, 0, 0));
	CHECK_FALSE(JumpArc::Build({ 0, 0, 0 }, { 0, 0, 0 }, 5, 10, 0, 0, 0));
}
