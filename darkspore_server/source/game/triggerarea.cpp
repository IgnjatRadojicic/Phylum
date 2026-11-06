
// Include
#include "triggerarea.h"

#include <algorithm>
#include <cmath>

// Game
namespace Game {
	TriggerArea TriggerArea::Sphere(const glm::vec3& center, float radius) {
		TriggerArea shape;
		shape.mCenter = center;
		shape.mRadius = std::max(radius, 0.f);
		return shape;
	}

	TriggerArea TriggerArea::Box(const glm::vec3& center, const glm::vec3& halfSize, const glm::vec3& facing) {
		TriggerArea shape;
		shape.mCenter = center;
		shape.mHalfSize = glm::max(halfSize, glm::vec3(0.f));
		shape.mIsBox = true;

		// The box stands upright and turns about the up axis only.
		const glm::vec2 ground(facing.x, facing.y);
		if (glm::length(ground) > 0.0001f) {
			const glm::vec2 forward = glm::normalize(ground);
			shape.mForward = glm::vec3(forward, 0.f);
			shape.mSide = glm::vec3(forward.y, -forward.x, 0.f);
		}
		return shape;
	}

	void TriggerArea::MoveTo(const glm::vec3& center) {
		mCenter = center;
	}

	const glm::vec3& TriggerArea::GetCenter() const {
		return mCenter;
	}

	float TriggerArea::GetReach() const {
		return mIsBox ? glm::length(mHalfSize) : mRadius;
	}

	bool TriggerArea::Touches(const glm::vec3& position, float bodyRadius) const {
		const glm::vec3 offset = position - mCenter;
		const float body = std::max(bodyRadius, 0.f);
		if (!mIsBox) {
			return glm::length(offset) <= mRadius + body;
		}

		return std::abs(glm::dot(offset, mSide)) <= mHalfSize.x + body
			&& std::abs(glm::dot(offset, mForward)) <= mHalfSize.y + body
			&& std::abs(offset.z) <= mHalfSize.z + body;
	}
}
