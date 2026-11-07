
// Include
#include "testenemyring.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

// Game
namespace Game {
	std::vector<glm::vec3> TestEnemyRing::Positions(const glm::vec3& center, uint32_t count, float distance) {
		std::vector<glm::vec3> positions;
		positions.reserve(count);

		const float radius = std::max(distance, 0.f);
		for (uint32_t i = 0; i < count; ++i) {
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(count);
			positions.emplace_back(center.x + radius * std::cos(angle), center.y + radius * std::sin(angle), center.z);
		}
		return positions;
	}
}
