
#include <doctest/doctest.h>

#include "game/walktimer.h"

using Game::WalkTimer;

TEST_CASE("a walk counts down at the move speed") {
	WalkTimer walk;
	walk.Start({ 0, 0, 0 }, { 10, 0, 0 }, 5.f);
	CHECK(walk.GetDistanceLeft() == doctest::Approx(10.f));
	walk.Advance(1.f);
	CHECK(walk.GetDistanceLeft() == doctest::Approx(5.f));
	CHECK(walk.IsWalking());
	walk.Advance(2.f);
	CHECK(walk.GetDistanceLeft() == doctest::Approx(0.f));
	CHECK_FALSE(walk.IsWalking());
}

TEST_CASE("mid-walk the object is part way along the line") {
	WalkTimer walk;
	walk.Start({ 2, 4, 1 }, { 2, 14, 3 }, 2.f);
	walk.Advance(1.f);
	const auto position = walk.GetPosition();
	CHECK(position.x == doctest::Approx(2.f));
	CHECK(position.y == doctest::Approx(6.f));
	CHECK(position.z == doctest::Approx(1.4f));
}

TEST_CASE("a finished, cleared or untimed walk is at its goal") {
	WalkTimer walk;
	walk.Start({ 0, 0, 0 }, { 0, 8, 0 }, 4.f);
	walk.Advance(5.f);
	CHECK(walk.GetPosition().y == doctest::Approx(8.f));

	walk.Start({ 0, 0, 0 }, { 0, 8, 0 }, 4.f);
	walk.Clear();
	CHECK_FALSE(walk.IsWalking());
	CHECK(walk.GetPosition().y == doctest::Approx(8.f));

	walk.Start({ 0, 0, 0 }, { 0, 8, 0 }, 0.f);
	CHECK_FALSE(walk.IsWalking());
	CHECK(walk.GetPosition().y == doctest::Approx(8.f));
}

TEST_CASE("a new walk during one starts where the client is on it") {
	WalkTimer walk;
	walk.Start({ 0, 0, 0 }, { 10, 0, 0 }, 5.f);
	walk.Advance(1.f);
	// The server stands on the old goal (10, 0); the client is at 5, 0.
	walk.Start({ 10, 0, 0 }, { 5, 6, 0 }, 3.f);
	CHECK(walk.GetDistanceLeft() == doctest::Approx(6.f));
	walk.Advance(1.f);
	CHECK(walk.GetPosition().x == doctest::Approx(5.f));
	CHECK(walk.GetPosition().y == doctest::Approx(3.f));
}

TEST_CASE("height does not lengthen a walk") {
	WalkTimer walk;
	walk.Start({ 0, 0, 0 }, { 3, 4, 50 }, 5.f);
	CHECK(walk.GetDistanceLeft() == doctest::Approx(5.f));
}

TEST_CASE("a walk that stops short ends its stop distance before the goal") {
	using Game::StopShort;
	const auto end = StopShort({ 0, 0, 0 }, { 10, 0, 0 }, 3.f);
	CHECK(end.x == doctest::Approx(7.f));
	CHECK(end.y == doctest::Approx(0.f));

	// Already within the distance it stays put; no distance walks all the way.
	CHECK(StopShort({ 1, 1, 0 }, { 2, 1, 0 }, 3.f) == glm::vec3(1, 1, 0));
	CHECK(StopShort({ 0, 0, 0 }, { 4, 3, 0 }, 0.f) == glm::vec3(4, 3, 0));
}

TEST_CASE("a negative speed is an untimed walk, not one that runs backwards") {
	Game::WalkTimer walk;
	walk.Start({ 0, 0, 0 }, { 10, 0, 0 }, -2.f);
	walk.Advance(1.f);
	CHECK_FALSE(walk.IsWalking());
	CHECK(walk.GetPosition() == glm::vec3(10, 0, 0));
}
