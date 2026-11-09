
#ifndef _GAME_HERODEPLOY_HEADER
#define _GAME_HERODEPLOY_HEADER

// Include
#include <cstdint>
#include <memory>
#include <string_view>

// Game
namespace Game {
	class Lua;
	class Object;
	class ServerEvent;

	// HeroDeploy
	// Runs the game's DeployModifier (Modifiers/modifier_deploy.lua) on a hero that arrives on the
	// field. No script requests it, so the engine did. It holds the hero (Immobilized and Immune)
	// for a second, plays character_beam_in with its element's beam effect, knocks nearby enemies
	// back at 0.5 s and gives the deploy bonus at 1 s. Its rank picks the effect: 1 is a swap's
	// beam-in, anything else the level entry (Darkspin's hero arrival picks them the same way).
	class HeroDeploy {
		public:
			enum class Arrival : int32_t {
				Swap = 1,
				Entry = 2
			};

			explicit HeroDeploy(Lua& lua);

			// Returns the modifier's instance id, or 0.
			uint32_t Start(const std::shared_ptr<Object>& hero, Arrival arrival);

			// The beam-out effect where a hero swapped away stood. No script plays it (the game's
			// BeamOutModifier is the level exit's character_exit_), so the engine did; the name is
			// the beam-in's with "out", as the game preloads them in StandardPreload.
			static ServerEvent BeamOut(const Object& hero);

			// The effect name's element part for an nDamageTypes value, as DeployModifier picks it.
			static std::string_view Element(uint32_t creatureType);

		private:
			Lua& mLua;
	};
}

#endif
