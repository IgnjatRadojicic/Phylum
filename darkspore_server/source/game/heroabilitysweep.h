
#ifndef _GAME_HERO_ABILITY_SWEEP_HEADER
#define _GAME_HERO_ABILITY_SWEEP_HEADER

// Include
#include <filesystem>
#include <string>

// Game
namespace Game {
	class Instance;
	class Lua;

	// Casts every hero's real abilities at a mob in an offline game and writes what each one did:
	// script errors, missing engine functions, damage, effects, objects and animations. It runs in
	// real time (a few seconds per ability), so it is a separate check (--check-heroes).
	class HeroAbilitySweep {
		public:
			// classFilter: only the hero classes whose name has it (empty: all).
			static void Run(Instance& game, Lua& lua, const std::filesystem::path& reportPath, const std::string& classFilter);
	};
}

#endif
