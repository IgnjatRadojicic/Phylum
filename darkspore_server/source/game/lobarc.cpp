
// Include
#include "lobarc.h"

#include <algorithm>
#include <cmath>

// Game
namespace Game {
	// LobArc
	std::optional<LobArc> LobArc::Build(const glm::vec3& start, const glm::vec3& destination, float height, float flightTime, float speed) {
		const glm::vec2 plane = glm::vec2(destination) - glm::vec2(start);
		const float planeDistance = glm::length(plane);
		if (planeDistance <= 0 || height < 0) {
			return std::nullopt;
		}

		const float duration = flightTime > 0 ? flightTime : (speed > 0 ? planeDistance / speed : 0);
		if (duration <= 0) {
			return std::nullopt;
		}

		LobArc arc;
		arc.mStart = start;
		arc.mDestination = destination;
		arc.mPlaneDirection = glm::vec3(plane / planeDistance, 0);
		arc.mHeight = height;
		arc.mDuration = duration;
		arc.mTotalDuration = duration;
		arc.mPlaneSpeed = planeDistance / duration;

		const float rise = destination.z - start.z;
		if (height == 0 && rise < 0) {
			// Nothing to climb, so it only falls.
			arc.mUpQuadratic = rise / (planeDistance * planeDistance);
			return arc;
		}

		// The top of the curve clears the higher of the two ends by the height asked for.
		const float apexHeight = height + std::max(rise, 0.f);
		float apexDistance = planeDistance * 0.5f;
		if (rise != 0) {
			const float discriminant = apexHeight * apexHeight - rise * apexHeight;
			if (discriminant < 0) {
				return std::nullopt;
			}
			apexDistance = (apexHeight - std::sqrt(discriminant)) / rise * planeDistance;
		}

		if (apexDistance <= 0) {
			return std::nullopt;
		}

		arc.mUpLinear = 2 * apexHeight / apexDistance;
		arc.mUpQuadratic = -apexHeight / (apexDistance * apexDistance);
		return arc;
	}

	void LobArc::AddBounces(int32_t count, float restitution) {
		mBounces = std::max(count, 0);
		mBounceRestitution = restitution;

		mTotalDuration = mDuration;
		float bounce = mDuration;
		for (int32_t index = 0; index < mBounces; ++index) {
			bounce *= restitution;
			mTotalDuration += bounce;
		}
	}

	glm::vec3 LobArc::GetPositionAt(float seconds) const {
		if (seconds >= mDuration) {
			// Bouncing carries it on along the ground past where it first landed.
			return mDestination + mPlaneDirection * (mPlaneSpeed * (std::min(seconds, mTotalDuration) - mDuration));
		}

		const float distance = mPlaneSpeed * std::max(seconds, 0.f);
		glm::vec3 position = mStart + mPlaneDirection * distance;
		position.z = mStart.z + mUpLinear * distance + mUpQuadratic * distance * distance;
		return position;
	}

	glm::vec3 LobArc::GetLanding() const {
		return GetPositionAt(mTotalDuration);
	}

	glm::vec3 LobArc::GetInitialVelocity() const {
		return glm::vec3(mPlaneDirection.x * mPlaneSpeed, mPlaneDirection.y * mPlaneSpeed, mUpLinear);
	}
}
