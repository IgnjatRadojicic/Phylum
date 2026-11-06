
// Include
#include "abilityevent.h"

// Game
namespace Game {
	namespace {
		bool InRange(size_t slot) {
			return slot >= 1 && slot <= AbilityEvent::SlotCount;
		}
	}

	AbilityEvent::AbilityEvent(AbilityEventType type) : mType(type) {}

	AbilityEventType AbilityEvent::GetType() const {
		return mType;
	}

	uint64_t AbilityEvent::GetGUID(size_t slot) const {
		return InRange(slot) ? mGUIDs[slot - 1] : 0;
	}

	float AbilityEvent::GetFloat(size_t slot) const {
		return InRange(slot) ? mFloats[slot - 1] : 0.f;
	}

	int64_t AbilityEvent::GetInt(size_t slot) const {
		return InRange(slot) ? mInts[slot - 1] : 0;
	}

	AbilityEvent& AbilityEvent::SetGUID(size_t slot, uint64_t value) {
		if (InRange(slot)) {
			mGUIDs[slot - 1] = value;
		}
		return *this;
	}

	AbilityEvent& AbilityEvent::SetFloat(size_t slot, float value) {
		if (InRange(slot)) {
			mFloats[slot - 1] = value;
		}
		return *this;
	}

	AbilityEvent& AbilityEvent::SetInt(size_t slot, int64_t value) {
		if (InRange(slot)) {
			mInts[slot - 1] = value;
		}
		return *this;
	}
}
