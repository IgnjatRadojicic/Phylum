
// Include
#include "aitargeting.h"

#include <algorithm>
#include <utility>

// Game
namespace Game {
	std::vector<uint32_t> OrderHostiles(const glm::vec3& from, uint8_t team, float radius, bool detectsStealth,
		const std::vector<TargetCandidate>& candidates, uint32_t selfId
	) {
		const float reach = radius * radius;

		std::vector<std::pair<float, uint32_t>> found;
		for (const auto& candidate : candidates) {
			if (candidate.id == selfId || !candidate.alive || !candidate.targetable || candidate.team == team) {
				continue;
			}
			if (candidate.stealthed && !detectsStealth) {
				continue;
			}

			const auto offset = candidate.position - from;
			const float distance = glm::dot(offset, offset);
			if (distance <= reach) {
				found.emplace_back(distance, candidate.id);
			}
		}

		// Distance, then id: the same order whatever order the candidates came in.
		std::sort(found.begin(), found.end());

		std::vector<uint32_t> ids;
		ids.reserve(found.size());
		for (const auto& [_, id] : found) {
			ids.push_back(id);
		}
		return ids;
	}

	uint32_t PickNearestHostile(const glm::vec3& from, uint8_t team, float radius, const std::vector<TargetCandidate>& candidates, uint32_t selfId) {
		const auto ids = OrderHostiles(from, team, radius, false, candidates, selfId);
		return ids.empty() ? 0 : ids.front();
	}
}
