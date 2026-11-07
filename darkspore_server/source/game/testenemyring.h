#ifndef _GAME_TESTENEMYRING_HEADER
#define _GAME_TESTENEMYRING_HEADER

// Include
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// Game
namespace Game {
	// TestEnemyRing
	// Where the TEST_ENEMIES_AT_SPAWN setting puts its enemies: evenly around a ring on the
	// ground, at the height of the centre. It is a testing aid, not part of the game: the
	// original levels never put enemies on the spawn island.
	// Approximate: the server has no navigation mesh yet, so a spot may fall past the edge of
	// a small island. The distance is kept short for that reason.
	class TestEnemyRing {
		public:
			static constexpr float defaultDistance = 6.f;

			static std::vector<glm::vec3> Positions(const glm::vec3& center, uint32_t count, float distance = defaultDistance);
	};
}

#endif
