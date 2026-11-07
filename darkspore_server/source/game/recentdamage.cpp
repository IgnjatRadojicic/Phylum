
// Include
#include "recentdamage.h"

// Game
namespace Game {
	void RecentDamage::Record(uint32_t victim, float amount, uint64_t time) {
		mHits.push_back(Hit { time, victim, amount });
		if (mHits.size() > capacity) {
			mHits.pop_front();
		}
	}

	float RecentDamage::Total(uint32_t victim, uint64_t since, float cap) const {
		float total = 0;
		for (const auto& hit : mHits) {
			if (hit.victim != victim || hit.time <= since) {
				continue;
			}

			total += hit.amount;
			if (total >= cap) {
				return cap;
			}
		}
		return total;
	}
}
