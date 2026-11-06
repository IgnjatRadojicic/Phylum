
// Include
#include "projectilesweep.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace {
	// Hits closer together than this count as the same distance, so rounding cannot decide
	// which of two stacked mobs is hit.
	constexpr float distanceStep = 0.001f;

	// How far along the path, from 0 to 1, a point first enters a box. Growing the target's box
	// by the projectile's half size lets the projectile count as a point.
	std::optional<float> FindEntry(const glm::vec3& from, const glm::vec3& path, const glm::vec3& minimum, const glm::vec3& maximum) {
		float entry = 0;
		float exit = 1;
		for (int axis = 0; axis < 3; ++axis) {
			if (path[axis] == 0) {
				if (from[axis] < minimum[axis] || from[axis] > maximum[axis]) {
					return std::nullopt;
				}
				continue;
			}

			const float first = (minimum[axis] - from[axis]) / path[axis];
			const float second = (maximum[axis] - from[axis]) / path[axis];
			entry = std::max(entry, std::min(first, second));
			exit = std::min(exit, std::max(first, second));
			if (entry > exit) {
				return std::nullopt;
			}
		}
		return entry;
	}
}

// Game
namespace Game {
	// ProjectileSweep
	ProjectileSweep::ProjectileSweep(const glm::vec3& from, const glm::vec3& to, const glm::vec3& halfExtent, uint32_t intendedId) :
		mFrom(from), mTo(to), mHalfExtent(halfExtent), mIntendedId(intendedId) {}

	std::vector<SweepHit> ProjectileSweep::Find(const std::vector<SweepTarget>& targets) const {
		const glm::vec3 path = mTo - mFrom;
		const float pathLength = glm::length(path);

		std::vector<SweepHit> hits;
		for (const auto& target : targets) {
			const glm::vec3 minimum = target.position + target.minimum - mHalfExtent;
			const glm::vec3 maximum = target.position + target.maximum + mHalfExtent;
			const auto entry = FindEntry(mFrom, path, minimum, maximum);
			if (entry) {
				hits.push_back({ target.id, *entry * pathLength, mFrom + path * *entry });
			}
		}

		const auto step = [](const SweepHit& hit) {
			return std::llround(hit.distance / distanceStep);
		};
		std::sort(hits.begin(), hits.end(), [&](const SweepHit& left, const SweepHit& right) {
			if (step(left) != step(right)) {
				return step(left) < step(right);
			}
			if ((left.id == mIntendedId) != (right.id == mIntendedId)) {
				return left.id == mIntendedId;
			}
			return left.id < right.id;
		});
		return hits;
	}
}
