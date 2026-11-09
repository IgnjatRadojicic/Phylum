
// Include
#include "walktimer.h"

#include <algorithm>

// Game
namespace Game {
	void WalkTimer::Start(const glm::vec3& from, const glm::vec3& to, float speed) {
		// A new walk while one is under way starts where the client is on the old one: the server
		// already stands on the old goal, the client not yet.
		mFrom = IsWalking() ? GetPosition() : from;
		mTo = to;
		// A slow below -100% gives a negative speed, which would walk the timer backwards.
		mSpeed = std::max(speed, 0.f);
		mLength = glm::length(glm::vec2(to.x - mFrom.x, to.y - mFrom.y));
		mDistanceLeft = mSpeed > 0 ? mLength : 0.f;
	}

	void WalkTimer::Advance(float seconds) {
		mDistanceLeft = std::max(0.f, mDistanceLeft - mSpeed * seconds);
	}

	void WalkTimer::Clear() {
		mDistanceLeft = 0;
	}

	float WalkTimer::GetDistanceLeft() const {
		return mDistanceLeft;
	}

	bool WalkTimer::IsWalking() const {
		return mDistanceLeft > 0;
	}

	glm::vec3 WalkTimer::GetPosition() const {
		if (mLength <= 0 || mDistanceLeft <= 0) {
			return mTo;
		}
		return glm::mix(mFrom, mTo, 1.f - mDistanceLeft / mLength);
	}

	glm::vec3 StopShort(const glm::vec3& from, const glm::vec3& goal, float stop) {
		const auto delta = goal - from;
		const float length = glm::length(delta);
		if (stop <= 0 || length <= 0) {
			return goal;
		}
		return length <= stop ? from : goal - delta * (stop / length);
	}
}
