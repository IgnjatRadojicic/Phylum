
// Include
#include "castpursuit.h"

// Game
namespace Game {
	// CastPursuit
	CastPursuit::Answer CastPursuit::Decide(const Cast& cast, uint64_t nowMs) {
		const auto chase = mChases.find(cast.casterId);
		// The client's arrival comes without a target. One that names it is the player casting again,
		// which is checked like any cast.
		const bool retry = chase != mChases.end() &&
			chase->second.abilityId == cast.abilityId &&
			cast.targetId == 0 &&
			nowMs <= chase->second.untilMs;
		const uint32_t chasedTarget = chase != mChases.end() ? chase->second.targetId : 0;
		if (chase != mChases.end()) {
			mChases.erase(chase);
		}

		if (retry) {
			return { Decision::Run, chasedTarget };
		}

		// The player casting at the chased target again gets a little slack, so a hero that stopped
		// just short is not sent walking again and again.
		const bool chasedAgain = chasedTarget != 0 && chasedTarget == cast.targetId;
		const float range = cast.range + (chasedAgain ? slack : 0.f);
		const bool outOfRange = cast.shouldPursue && cast.targetId != 0 && cast.range > 0 && cast.gap > range;
		if (!outOfRange) {
			return { Decision::Run, cast.targetId };
		}

		mChases[cast.casterId] = { cast.abilityId, cast.targetId, nowMs + timeoutMs };
		return { Decision::Pursue, cast.targetId };
	}

	void CastPursuit::Forget(uint32_t casterId) {
		mChases.erase(casterId);
	}
}
