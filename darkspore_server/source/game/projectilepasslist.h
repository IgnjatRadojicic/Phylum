
#ifndef _GAME_PROJECTILEPASSLIST_HEADER
#define _GAME_PROJECTILEPASSLIST_HEADER

// Include
#include <array>
#include <cstddef>
#include <cstdint>

// Game
namespace Game {
	// ProjectilePassList
	// The creatures a projectile has gone through, as the client's hit test decides it (0xa2f7f0):
	// a target with Incorporeal above 0 that wins a roll against it, or any target with Intangible,
	// lets the shot pass, and from then on always does. The client keeps 8 of them (0xa2f7c0); with
	// the list full a passing target is not written down, so it rolls again next time.
	class ProjectilePassList {
		public:
			static constexpr size_t Capacity = 8;

			enum class Result {
				Hits,
				Passes,
				// Passes, and has just been written down: the client raises Incorporeal then.
				PassesFirstTime
			};

			// The roll is from 0 up to 1.
			Result Check(uint32_t target, float incorporeal, float intangible, float roll);

		private:
			std::array<uint32_t, Capacity> mIds {};
	};
}

#endif
