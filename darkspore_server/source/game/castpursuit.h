
#ifndef _GAME_CAST_PURSUIT_HEADER
#define _GAME_CAST_PURSUIT_HEADER

// Include
#include <cstdint>
#include <unordered_map>

// Game
namespace Game {
	// CastPursuit
	// A hero's cast of a "shouldPursue" ability at a target out of its range does not run. The server
	// answers "pursue": the client walks the hero to the target and sends the same cast again on
	// arrival (Darkspin, from the client's sub_9E0660) without the target, which the chase remembers.
	// That arrival runs wherever the hero is, so a moving target cannot cause an endless chase.
	class CastPursuit {
		public:
			static constexpr uint64_t timeoutMs = 8000;
			// Metres beyond range a repeat cast at the chased target still runs (Darkspin's redirect is 0.75).
			static constexpr float slack = 1.f;

			struct Cast {
				uint32_t casterId = 0;
				uint32_t abilityId = 0;
				uint32_t targetId = 0;
				bool shouldPursue = false;
				// The ability's range, and the gap between the caster's and the target's edges.
				float range = 0;
				float gap = 0;
			};

			enum class Decision { Run, Pursue };

			struct Answer {
				Decision decision = Decision::Run;
				// The target the cast runs at: the chased one for a retry that came without one.
				uint32_t targetId = 0;
			};

			Answer Decide(const Cast& cast, uint64_t nowMs);

			// The hero cast something else, swapped or cancelled: a later retry is a new cast.
			void Forget(uint32_t casterId);

		private:
			struct Chase {
				uint32_t abilityId = 0;
				uint32_t targetId = 0;
				uint64_t untilMs = 0;
			};

			std::unordered_map<uint32_t, Chase> mChases;
	};
}

#endif
