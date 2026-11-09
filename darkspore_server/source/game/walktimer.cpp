
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
		mSpeed = speed;
		mLength = glm::length(glm::vec2(to.x - mFrom.x, to.y - mFrom.y));
		mDistanceLeft = speed > 0 ? mLength : 0.f;
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
}
