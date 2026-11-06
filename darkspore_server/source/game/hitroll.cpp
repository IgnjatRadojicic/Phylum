
// Include
#include "hitroll.h"

#include <algorithm>

// Game
namespace Game {
	float HitRoll::AvoidChance(float rating, float ratingPerPercent, float cap) {
		if (rating <= 0 || ratingPerPercent <= 0) {
			return 0;
		}
		// As the tooltip does it: min(rating / perPercent, cap * 100) percent
		return std::min(rating / ratingPerPercent, cap * 100.f) / 100.f;
	}

	HitOutcome HitRoll::Roll(bool physical, bool overTime, const HitDefense& defense,
		float ratingPerPercent, float dodgeCap, float resistCap, float random
	) {
		if (defense.immune) {
			return HitOutcome::Immune;
		}
		if (overTime) {
			return HitOutcome::Hit;
		}

		if (physical) {
			if (random < AvoidChance(defense.physicalDefense, ratingPerPercent, dodgeCap)) {
				return HitOutcome::Dodged;
			}
		} else if (random < AvoidChance(defense.energyDefense, ratingPerPercent, resistCap)) {
			return HitOutcome::Resisted;
		}
		return HitOutcome::Hit;
	}

	float HitRoll::CriticalChance(float criticalRating, float ratingPerPercent) {
		const float perPercent = ratingPerPercent > 0 ? ratingPerPercent : 1.f;
		return std::clamp(criticalRating / (perPercent * 100.f), 0.f, 1.f);
	}

	float HitRoll::CriticalMultiplier(float criticalDamageBonus, float criticalDamageIncrease) {
		return std::max(1.f, criticalDamageBonus + criticalDamageIncrease);
	}
}
