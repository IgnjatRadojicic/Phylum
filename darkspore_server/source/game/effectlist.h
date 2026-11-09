#ifndef _GAME_EFFECTLIST_HEADER
#define _GAME_EFFECTLIST_HEADER

// Include
#include <cstdint>
#include <map>
#include <vector>

// Game
namespace Game {
	// EffectList
	class EffectList {
		public:
			uint8_t Add(uint32_t effect);
			uint8_t Remove(uint32_t effect);
			bool RemoveByIndex(uint8_t index);

		private:
			uint8_t GetOpenIndex();
			void FreeIndex(uint8_t index);

		private:
			std::map<uint32_t, std::vector<uint8_t>> mIndexesByEffect;

			std::vector<uint8_t> mOpenIndexes;
			std::vector<uint8_t> mUsedIndexes;
	};
}

#endif
