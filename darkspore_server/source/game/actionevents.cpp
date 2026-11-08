
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

	void ActionEvents::StartedNewAction(uint32_t agent) {
		SendPlain(agent, AbilityEventType::StartedNewAction);
	}

	void ActionEvents::BreakRoot(uint32_t object) {
		SendPlain(object, AbilityEventType::BreakRoot);
	}

	void ActionEvents::LostModifierOfType(uint32_t agent, uint32_t guid) {
		if (agent == 0) {
			return;
		}

		AbilityEvent event(AbilityEventType::LostModifierOfType);
		event.SetGUID(1, guid);
		mSink.SendEvent(agent, event);
	}

	void ActionEvents::PlayerSwap(uint32_t outgoing) {
		SendPlain(outgoing, AbilityEventType::PlayerSwap);
	}

	void ActionEvents::Incorporeal(uint32_t target, uint32_t projectile) {
		if (target == 0) {
			return;
		}

		AbilityEvent event(AbilityEventType::Incorporeal);
		event.SetGUID(1, target).SetGUID(2, projectile);
		mSink.SendEvent(target, event);
	}

	void ActionEvents::SendPlain(uint32_t agent, AbilityEventType type) {
		if (agent != 0) {
			mSink.SendEvent(agent, AbilityEvent(type));
		}
	}
}
