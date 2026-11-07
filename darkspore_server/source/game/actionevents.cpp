
// Include
#include "actionevents.h"

// Game
namespace Game {
	ActionEvents::ActionEvents(AbilityEventSink& sink) : mSink(sink) {}

	void ActionEvents::UsedAbility(uint32_t agent, uint32_t descriptors) {
		if (agent == 0) {
			return;
		}

		AbilityEvent event(AbilityEventType::UsedAbility);
		event.SetInt(2, descriptors);
		mSink.SendEvent(agent, event);
	}

	void ActionEvents::NearbyDeath(uint32_t dead, const std::vector<DeathWitness>& witnesses) {
		if (dead == 0) {
			return;
		}

		for (const auto& witness : witnesses) {
			AbilityEvent event(AbilityEventType::NearbyDeath);
			event.SetGUID(1, dead).SetFloat(2, witness.distance);
			mSink.SendEvent(witness.id, event);
		}
	}
}
