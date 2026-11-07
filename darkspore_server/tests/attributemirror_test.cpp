#include <doctest/doctest.h>

#include "game/attributemirror.h"

using Game::AttributeMirror;
using Game::AttributeType;

namespace {
	const AttributeMirror::Stat& StatFor(AttributeType type) {
		for (const auto& stat : AttributeMirror::stats) {
			if (stat.type == type) {
				return stat;
			}
		}
		FAIL("not mirrored");
		return AttributeMirror::stats[0];
	}
}

TEST_CASE("a pet's mirrored stat is its class share of the source's unbuffed value") {
	CHECK(AttributeMirror::Value(StatFor(AttributeType::Strength), 0.5f, 80, 0.3f) == doctest::Approx(40));
	CHECK(AttributeMirror::Value(StatFor(AttributeType::CriticalRating), 0, 12, 0) == doctest::Approx(0));
}

TEST_CASE("only max health also grows with the source's PetHealth") {
	CHECK(AttributeMirror::Value(StatFor(AttributeType::MaxHealth), 0.5f, 400, 0.25f) == doctest::Approx(250));
	CHECK(AttributeMirror::Value(StatFor(AttributeType::PhysicalDefense), 0.5f, 400, 0.25f) == doctest::Approx(200));
}

TEST_CASE("the pet keeps the same part of its health") {
	CHECK(AttributeMirror::Health(0.5f, 0.5f, 250) == doctest::Approx(250));
	CHECK(AttributeMirror::Health(25, 100, 300) == doctest::Approx(75));
	CHECK(AttributeMirror::Health(0, 0, 300) == doctest::Approx(300));
}

TEST_CASE("the eight stats the client mirrors") {
	CHECK(AttributeMirror::stats.size() == 8);
	CHECK(StatFor(AttributeType::MovementSpeedBuff).share == Game::ClassAttribute::MovementSpeedBuff);
}
