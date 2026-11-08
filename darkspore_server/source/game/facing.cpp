
// Include
#include "facing.h"

#include <algorithm>
#include <cmath>

// Game
namespace Game {
	std::optional<glm::quat> OrientationFromDirection(const glm::vec3& direction) {
		const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
		if (!(length > 0.0001f)) {
			return std::nullopt;
		}
		// Turning +Y by this angle about +Z gives (-sin, cos), which is the direction.
		const float angle = std::atan2(-direction.x, direction.y);
		return glm::angleAxis(angle, glm::vec3(0.f, 0.f, 1.f));
	}

	float AngleBetween(const glm::quat& from, const glm::quat& to) {
		const float cosine = std::min(1.f, std::abs(glm::dot(glm::normalize(from), glm::normalize(to))));
		return 2.f * std::acos(cosine);
	}
}
