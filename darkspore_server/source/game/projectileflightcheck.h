
#ifndef _GAME_PROJECTILEFLIGHTCHECK_HEADER
#define _GAME_PROJECTILEFLIGHTCHECK_HEADER

// Include
#include <filesystem>

// Game
namespace Game {
	class Instance;
	class Lua;

	// ProjectileFlightCheck
	// Fires real projectiles at real mobs through the script adapter and the game's tick, with
	// no game client, and writes whether each shot hit what it should.
	class ProjectileFlightCheck {
		public:
			static void Run(Instance& game, Lua& lua, const std::filesystem::path& reportPath);
	};
}

#endif
