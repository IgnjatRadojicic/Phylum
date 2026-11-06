
#ifndef _GAME_TRIGGEROCCUPANCY_HEADER
#define _GAME_TRIGGEROCCUPANCY_HEADER

// Include
#include <cstdint>
#include <set>
#include <vector>

// Game
namespace Game {
	// TriggerEvents
	// What changed in a trigger volume since the last tick, by object id.
	struct TriggerEvents {
		std::vector<uint32_t> entered;
		std::vector<uint32_t> stayed;
		std::vector<uint32_t> exited;
	};

	// TriggerOccupancy
	// Which objects are in one trigger volume. As the client's volumes do it (Darkspore.exe
	// 0xa16810, 0xa168a0, 0xa16960): enter once on the way in, stay on each later tick while
	// inside, exit once on the way out.
	class TriggerOccupancy {
		public:
			// inside is every object in the volume now. An id listed twice counts once.
			TriggerEvents Update(const std::vector<uint32_t>& inside);

			// An object leaves the world. True when it was inside, so it exits now.
			bool Remove(uint32_t id);

			bool Contains(uint32_t id) const;

		private:
			std::set<uint32_t> mInside;
	};
}

#endif
