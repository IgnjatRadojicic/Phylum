#include <doctest/doctest.h>

#include "game/testenemyring.h"

#include <glm/glm.hpp>

using Game::TestEnemyRing;

TEST_CASE("test enemies stand on a ring around the spawn point, at its height") {
	const glm::vec3 center(100, 50, 7);
	const auto positions = TestEnemyRing::Positions(center, 3, 6);
	REQUIRE(positions.size() == 3);
	for (const auto& position : positions) {
		CHECK(glm::distance(glm::vec2(position), glm::vec2(center)) == doctest::Approx(6));
		CHECK(position.z == doctest::Approx(7));
	}
}

TEST_CASE("test enemies are spread out, not stacked on one spot") {
	const auto positions = TestEnemyRing::Positions({ 0, 0, 0 }, 4, 6);
	REQUIRE(positions.size() == 4);
	for (size_t i = 0; i < positions.size(); ++i) {
		for (size_t j = i + 1; j < positions.size(); ++j) {
			CHECK(glm::distance(positions[i], positions[j]) > 1.f);
		}
	}
}

TEST_CASE("no test enemies gives no positions, and a negative distance puts them on the spawn point") {
	CHECK(TestEnemyRing::Positions({ 0, 0, 0 }, 0).empty());

	const auto positions = TestEnemyRing::Positions({ 1, 2, 3 }, 2, -5);
	REQUIRE(positions.size() == 2);
	CHECK(glm::distance(positions[0], glm::vec3(1, 2, 3)) == doctest::Approx(0));
}
