
#include <doctest/doctest.h>

#include "game/facing.h"

#include <cmath>

using Game::AngleBetween;
using Game::OrientationFromDirection;

namespace {
	glm::vec3 Forward(const glm::quat& orientation) {
		return orientation * glm::vec3(0.f, 1.f, 0.f);
	}
}

TEST_CASE("an orientation looks along the direction it was made from") {
	for (const glm::vec3 direction : { glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0), glm::vec3(3, 4, 0) }) {
		const auto orientation = OrientationFromDirection(direction);
		REQUIRE(orientation);
		const auto forward = Forward(*orientation);
		const auto expected = glm::normalize(direction);
		CHECK(forward.x == doctest::Approx(expected.x));
		CHECK(forward.y == doctest::Approx(expected.y));
		CHECK(forward.z == doctest::Approx(0.f));
	}
}

TEST_CASE("+Y is no turn at all") {
	const auto orientation = OrientationFromDirection({ 0, 5, 0 });
	REQUIRE(orientation);
	CHECK(AngleBetween(*orientation, glm::identity<glm::quat>()) == doctest::Approx(0.f));
}

TEST_CASE("height does not tilt the orientation") {
	const auto flat = OrientationFromDirection({ 1, 1, 0 });
	const auto raised = OrientationFromDirection({ 1, 1, 5 });
	REQUIRE(flat);
	REQUIRE(raised);
	CHECK(AngleBetween(*flat, *raised) == doctest::Approx(0.f));
}

TEST_CASE("a direction with no length across the ground gives nothing") {
	CHECK_FALSE(OrientationFromDirection({ 0, 0, 0 }));
	CHECK_FALSE(OrientationFromDirection({ 0, 0, 1 }));
	CHECK_FALSE(OrientationFromDirection({ NAN, NAN, 0 }));
}

TEST_CASE("the angle between orientations is the turn from one to the other") {
	const auto north = OrientationFromDirection({ 0, 1, 0 });
	const auto east = OrientationFromDirection({ 1, 0, 0 });
	const auto south = OrientationFromDirection({ 0, -1, 0 });
	REQUIRE(north);
	REQUIRE(east);
	REQUIRE(south);
	CHECK(AngleBetween(*north, *east) == doctest::Approx(3.14159265f / 2));
	CHECK(AngleBetween(*north, *south) == doctest::Approx(3.14159265f));
}
