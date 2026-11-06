
// Include
#include "projectilesweep.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace {
	// Hits closer together than this count as the same distance, so rounding cannot decide
	// which of two stacked mobs is hit.
	constexpr float distanceStep = 0.001f;

	// How far along the path, from 0 to 1, a circle is first touched. The game is played on a
	// plane, and a shot's height should not make it miss a mob it visibly crosses.
	std::optional<float> FindEntry(const glm::vec2& from, const glm::vec2& to, const glm::vec2& center, float radius) {
		const glm::vec2 path = to - from;
		const glm::vec2 toCenter = center - from;
		const float distanceSquared = glm::dot(toCenter, toCenter);
		const float radiusSquared = radius * radius;
		if (distanceSquared <= radiusSquared) {
			return 0.f;
		}

		const float lengthSquared = glm::dot(path, path);
		if (lengthSquared == 0) {
			return std::nullopt;
		}

		const float along = glm::dot(toCenter, path);
		const float discriminant = along * along - lengthSquared * (distanceSquared - radiusSquared);
		if (discriminant < 0) {
			return std::nullopt;
		}

		const float entry = (along - std::sqrt(discriminant)) / lengthSquared;
		if (entry < 0 || entry > 1) {
			return std::nullopt;
		}
		return entry;
	}
}

// Game
namespace Game {
	// ProjectileSweep
	ProjectileSweep::ProjectileSweep(const glm::vec3& from, const glm::vec3& to, float radius, uint32_t intendedId) :
		mFrom(from), mTo(to), mRadius(radius), mIntendedId(intendedId) {}

	std::vector<SweepHit> ProjectileSweep::Find(const std::vector<SweepTarget>& targets) const {
		const float pathLength = glm::length(mTo - mFrom);

		std::vector<SweepHit> hits;
		for (const auto& target : targets) {
			const auto entry = FindEntry(mFrom, mTo, target.position, mRadius + target.radius);
			if (entry) {
				hits.push_back({ target.id, *entry * pathLength, mFrom + (mTo - mFrom) * *entry });
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
