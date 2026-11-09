// Include
#include "effectlist.h"

#include <algorithm>

// Game
namespace Game {
	// EffectList
	// One effect can be on an object several times (LightningRogueSupport adds an orb effect per orb),
	// each copy at its own index; removing the effect takes the newest copy.
	uint8_t EffectList::Add(uint32_t effect) {
		uint8_t index = GetOpenIndex();
		if (index != 0) {
			mIndexesByEffect[effect].push_back(index);
		}
		return index;
	}

	uint8_t EffectList::Remove(uint32_t effect) {
		auto mapIt = mIndexesByEffect.find(effect);
		if (mapIt == mIndexesByEffect.end()) {
			return 0;
		}

		const uint8_t index = mapIt->second.back();
		mapIt->second.pop_back();
		if (mapIt->second.empty()) {
			mIndexesByEffect.erase(mapIt);
		}
		FreeIndex(index);
		return index;
	}

	bool EffectList::RemoveByIndex(uint8_t index) {
		if (std::find(mUsedIndexes.begin(), mUsedIndexes.end(), index) == mUsedIndexes.end()) {
			return false;
		}

		for (auto mapIt = mIndexesByEffect.begin(); mapIt != mIndexesByEffect.end(); ++mapIt) {
			auto& indexes = mapIt->second;
			if (const auto it = std::find(indexes.begin(), indexes.end(), index); it != indexes.end()) {
				indexes.erase(it);
				if (indexes.empty()) {
					mIndexesByEffect.erase(mapIt);
				}
				break;
			}
		}
		FreeIndex(index);
		return true;
	}

	void EffectList::FreeIndex(uint8_t index) {
		if (const auto it = std::find(mUsedIndexes.begin(), mUsedIndexes.end(), index); it != mUsedIndexes.end()) {
			std::swap(*it, mUsedIndexes.back());
			mUsedIndexes.pop_back();
			mOpenIndexes.push_back(index);
		}
	}

	uint8_t EffectList::GetOpenIndex() {
		uint8_t index;
		if (mOpenIndexes.empty()) {
			if (mUsedIndexes.size() < 0xFF) {
				mUsedIndexes.push_back(static_cast<uint8_t>(mUsedIndexes.size()) + 1);
				index = mUsedIndexes.back();
			} else {
				index = 0;
			}
		} else {
			index = mOpenIndexes.back();
			mOpenIndexes.pop_back();
			mUsedIndexes.push_back(index);
		}
		return index;
	}
}
