
// Include
#include "jumparc.h"

#include <algorithm>

// Game
namespace Game {
	std::optional<JumpArc> JumpArc::Build(const glm::vec3& start, const glm::vec3& direction, float distance, float speed,
		float minHeight, float maxHeight, float rangeToHitMaxHeight
	) {
		const float length = glm::length(direction);
		if (distance <= 0 || speed <= 0 || length <= 0.0001f) {
			return std::nullopt;
		}

		JumpArc arc;
		arc.mStart = start;
		arc.mDirection = direction / length;
		arc.mDestination = start + arc.mDirection * distance;
		arc.mDuration = distance / speed;

		const float low = std::max(minHeight, 0.f);
		const float high = std::max(maxHeight, low);
		const float reach = rangeToHitMaxHeight > 0 ? std::clamp(distance / rangeToHitMaxHeight, 0.f, 1.f) : 1.f;
		arc.mHeight = low + (high - low) * reach;
		return arc;
	}

	glm::vec3 JumpArc::GetPositionAt(float seconds) const {
		const float t = mDuration > 0 ? std::clamp(seconds / mDuration, 0.f, 1.f) : 1.f;
		glm::vec3 position = mStart + (mDestination - mStart) * t;
		position.z += 4.f * mHeight * t * (1.f - t);
		return position;
	}

	glm::vec3 JumpArc::GetGroundAt(float seconds) const {
		const float t = mDuration > 0 ? std::clamp(seconds / mDuration, 0.f, 1.f) : 1.f;
		return mStart + (mDestination - mStart) * t;
	}

	bool JumpArc::IsFinished(float seconds) const {
		return seconds >= mDuration;
	}

	const glm::vec3& JumpArc::GetDestination() const {
		return mDestination;
	}

	const glm::vec3& JumpArc::GetDirection() const {
		return mDirection;
	}

	float JumpArc::GetHeight() const {
		return mHeight;
	}

	float JumpArc::GetDuration() const {
		return mDuration;
	}
}
