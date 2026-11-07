
#ifndef _GAME_RECENTDAMAGE_HEADER
#define _GAME_RECENTDAMAGE_HEADER

// Include
#include <cstddef>
#include <cstdint>
#include <deque>

// Game
namespace Game {
	// RecentDamage
	// The latest hits of the whole game, for nGameObject.GetRecentDamage. Like the client, it keeps
	// the last 1024 hits and adds up what one object took, stopping at a cap.
	// Approximate: the client also marks some hits to leave out unless asked; what marks them is
	// not known, so every hit counts.
	class RecentDamage {
		public:
			static constexpr size_t capacity = 1024;

			void Record(uint32_t victim, float amount, uint64_t time);

			// The damage the victim took after the given time, at most cap.
			float Total(uint32_t victim, uint64_t since, float cap) const;

		private:
			struct Hit {
				uint64_t time = 0;
				uint32_t victim = 0;
				float amount = 0;
			};

			std::deque<Hit> mHits;
	};
}

#endif
