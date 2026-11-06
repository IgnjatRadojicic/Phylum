
// Include
#include "triggeroccupancy.h"

// Game
namespace Game {
	TriggerEvents TriggerOccupancy::Update(const std::vector<uint32_t>& inside) {
		const std::set<uint32_t> now(inside.begin(), inside.end());

		TriggerEvents events;
		for (uint32_t id : now) {
			if (mInside.contains(id)) {
				events.stayed.push_back(id);
			} else {
				events.entered.push_back(id);
			}
		}
		for (uint32_t id : mInside) {
			if (!now.contains(id)) {
				events.exited.push_back(id);
			}
		}

		mInside = now;
		return events;
	}

	bool TriggerOccupancy::Remove(uint32_t id) {
		return mInside.erase(id) > 0;
	}

	bool TriggerOccupancy::Contains(uint32_t id) const {
		return mInside.contains(id);
	}
}
