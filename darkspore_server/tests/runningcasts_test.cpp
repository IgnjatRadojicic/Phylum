#include <doctest/doctest.h>

#include "game/runningcasts.h"

using namespace Game;

namespace {
	// The registry never looks inside a thread, so any distinct addresses do.
	LuaThread* FakeThread(uintptr_t n) {
		return reinterpret_cast<LuaThread*>(n * 16);
	}
}

TEST_CASE("an agent's casts come back in the order they started") {
	RunningCasts casts;
	casts.Add(7, FakeThread(1));
	casts.Add(7, FakeThread(2));
	casts.Add(9, FakeThread(3));

	const auto running = casts.Of(7);
	REQUIRE(running.size() == 2);
	CHECK(running[0].thread == FakeThread(1));
	CHECK(running[1].thread == FakeThread(2));
	CHECK(casts.Of(9).size() == 1);
	CHECK(casts.Of(5).empty());
}

TEST_CASE("an ended cast is no longer running, and its agent has none left") {
	RunningCasts casts;
	casts.Add(7, FakeThread(1));
	const auto entry = casts.Of(7)[0];

	casts.Remove(FakeThread(1));
	CHECK_FALSE(casts.IsRunning(entry));
	CHECK_FALSE(casts.Any(7));
	CHECK(casts.Of(7).empty());
}

TEST_CASE("a thread that carries a new cast is not the old cast") {
	RunningCasts casts;
	casts.Add(7, FakeThread(1));
	const auto old = casts.Of(7)[0];

	casts.Remove(FakeThread(1));
	casts.Add(7, FakeThread(1));

	CHECK_FALSE(casts.IsRunning(old));
	CHECK(casts.IsRunning(casts.Of(7)[0]));
}

TEST_CASE("a thread given a second cast keeps only the second") {
	RunningCasts casts;
	casts.Add(7, FakeThread(1));
	casts.Add(9, FakeThread(1));

	CHECK(casts.Of(7).empty());
	REQUIRE(casts.Of(9).size() == 1);
}

TEST_CASE("no agent or no thread registers nothing") {
	RunningCasts casts;
	casts.Add(0, FakeThread(1));
	casts.Add(7, nullptr);
	CHECK_FALSE(casts.Any(0));
	CHECK_FALSE(casts.Any(7));
	casts.Remove(FakeThread(4));
}
