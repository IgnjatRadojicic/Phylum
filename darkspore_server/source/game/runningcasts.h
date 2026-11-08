
#ifndef _GAME_RUNNINGCASTS_HEADER
#define _GAME_RUNNINGCASTS_HEADER

// Include
#include <cstdint>
#include <unordered_map>
#include <vector>

// Game
namespace Game {
	class LuaThread;

	// RunningCasts
	// The casts each agent has running, in the order they started, so events reach them as the
	// client's do (its agent keeps them in a list, 0x9deca0). A cast runs on one thread, which goes
	// back to the pool when the cast ends and can then carry another, so each cast gets its own
	// serial: an entry taken before a handler ran tells whether it is still the same cast.
	class RunningCasts {
		public:
			struct Entry {
				LuaThread* thread = nullptr;
				uint64_t serial = 0;
			};

			// A thread that already carries a cast gives it up for the new one.
			void Add(uint32_t agent, LuaThread* thread);
			void Remove(LuaThread* thread);

			// A copy, because handlers start and end casts.
			std::vector<Entry> Of(uint32_t agent) const;
			bool IsRunning(const Entry& entry) const;
			bool Any(uint32_t agent) const;

		private:
			struct Cast {
				uint32_t agent = 0;
				uint64_t serial = 0;
			};

			std::unordered_map<uint32_t, std::vector<Entry>> mByAgent;
			std::unordered_map<LuaThread*, Cast> mByThread;
			uint64_t mNextSerial = 1;
	};
}

#endif
