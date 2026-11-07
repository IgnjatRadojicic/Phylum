
// Include
#include "damageevents.h"

// Game
namespace Game {
	namespace {
		// Clears the sending flag however the handlers return.
		class SendingScope {
			public:
				explicit SendingScope(bool& sending) : mSending(sending) { mSending = true; }
				~SendingScope() { mSending = false; }

				SendingScope(const SendingScope&) = delete;
				SendingScope& operator=(const SendingScope&) = delete;

			private:
				bool& mSending;
		};
	}

	DamageEvents::DamageEvents(AbilityEventSink& sink) : mSink(sink) {}

	void DamageEvents::Report(const DamageReport& hit) {
		if (mSending || hit.victim == 0 || hit.amount <= 0.f) {
			return;
		}

		SendingScope scope(mSending);
		mSink.SendEvent(hit.victim, Describe(AbilityEventType::TookDamage, hit.attacker, hit));
		if (hit.attacker != 0) {
			mSink.SendEvent(hit.attacker, Describe(AbilityEventType::DealtDamage, hit.victim, hit));
		}
	}

	void DamageEvents::ReportDodge(uint32_t victim, uint32_t attacker) {
		if (mSending || victim == 0) {
			return;
		}

		SendingScope scope(mSending);
		AbilityEvent event(AbilityEventType::Dodged);
		event.SetGUID(1, victim).SetGUID(2, attacker);
		mSink.SendEvent(victim, event);
	}

	AbilityEvent DamageEvents::Describe(AbilityEventType type, uint32_t otherSide, const DamageReport& hit) const {
		AbilityEvent event(type);
		event.SetGUID(1, otherSide)
			.SetFloat(2, hit.amount)
			.SetInt(3, hit.descriptors)
			.SetInt(4, hit.damageType)
			.SetInt(5, hit.damageSource)
			.SetGUID(6, hit.sourceModifier)
			.SetGUID(7, hit.ability);
		return event;
	}
}
