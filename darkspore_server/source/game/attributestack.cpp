
// Include
#include "attributestack.h"

#include "attributes.h"

#include <algorithm>
#include <limits>

// Game
namespace Game {
	// AttributeStack
	uint32_t AttributeStack::Add(uint8_t attribute, float value, AttributeOp op, float currentValue) {
		auto [it, inserted] = mEntries.try_emplace(attribute);
		if (inserted) {
			it->second.base = currentValue;
		}

		const auto handle = mNextHandle++;
		it->second.changes.push_back({ handle, value, op });
		return handle;
	}

	std::optional<uint8_t> AttributeStack::Remove(uint32_t handle) {
		for (auto& [attribute, entry] : mEntries) {
			auto& changes = entry.changes;
			auto it = std::find_if(changes.begin(), changes.end(), [handle](const auto& change) { return change.handle == handle; });
			if (it != changes.end()) {
				changes.erase(it);
				return attribute;
			}
		}
		return std::nullopt;
	}

	bool AttributeStack::Has(uint8_t attribute) const {
		const auto it = mEntries.find(attribute);
		return it != mEntries.end() && !it->second.changes.empty();
	}

	void AttributeStack::SetBase(uint8_t attribute, float value) {
		if (auto it = mEntries.find(attribute); it != mEntries.end()) {
			it->second.base = value;
		}
	}

	float AttributeStack::GetBase(uint8_t attribute) const {
		const auto it = mEntries.find(attribute);
		return it != mEntries.end() ? it->second.base : 0.f;
	}

	float AttributeStack::Resolve(uint8_t attribute) const {
		const auto it = mEntries.find(attribute);
		if (it == mEntries.end()) {
			return 0.f;
		}

		float added = 0;
		float percent = 0;
		std::optional<float> bestAdd;
		std::optional<float> bestPercent;
		float limit = std::numeric_limits<float>::max();

		for (const auto& change : it->second.changes) {
			switch (change.op) {
				case AttributeOp::Add:
					added += change.value;
					break;
				case AttributeOp::PercentChange:
					percent += change.value;
					break;
				case AttributeOp::Clamp:
					limit = std::min(limit, change.value);
					break;
				case AttributeOp::AddBest:
					bestAdd = std::max(bestAdd.value_or(change.value), change.value);
					break;
				case AttributeOp::PercentChangeBest:
					bestPercent = std::max(bestPercent.value_or(change.value), change.value);
					break;
			}
		}

		const float value = (it->second.base + added + bestAdd.value_or(0.f)) * (1.f + percent + bestPercent.value_or(0.f));
		return std::min(value, limit);
	}

	// Functions
	uint8_t AttributeFromScript(uint8_t scriptAttribute) {
		constexpr auto id = [](AttributeType type) { return static_cast<uint8_t>(type); };

		switch (scriptAttribute) {
			case 74: return id(AttributeType::ImmuneToShocked);
			case 76: return id(AttributeType::ImmuneToTaunted);
			case 100: return id(AttributeType::DistributeDamageAmongSquad);
			case 114: return id(AttributeType::DefenseBoostPhysicalDamage);
		}

		if (scriptAttribute < 74) {
			return scriptAttribute;
		} else if (scriptAttribute == 75) {
			return scriptAttribute - 1;
		} else if (scriptAttribute < 100) {
			return scriptAttribute - 2;
		} else if (scriptAttribute < 114) {
			return scriptAttribute - 3;
		}
		return scriptAttribute;
	}
}
