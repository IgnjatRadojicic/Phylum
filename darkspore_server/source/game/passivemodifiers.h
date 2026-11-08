
#ifndef _GAME_PASSIVEMODIFIERS_HEADER
#define _GAME_PASSIVEMODIFIERS_HEADER

// Include
#include <cstdint>
#include <memory>

// Game
namespace Game {
	class Lua;
	class Noun;
	class Object;

	// PassiveModifiers
	// Starts the modifier an object's class names as always on. The game's scripts only ever
	// look these up (GetFirstModifierByGUID), never request them, so the engine applies them.
	//   Heroes: mpClassAttributes in the player class, when it names a modifier.
	//   Everything else: passiveAbility in the AI definition.
	// They start at rank 1, because nothing in the data gives a rank for them.
	class PassiveModifiers {
		public:
			explicit PassiveModifiers(Lua& lua);

			// The modifier the class names, or 0. Not yet checked against the loaded scripts.
			static uint32_t NamedBy(const Noun& noun);

			// Starts it on the object unless it is not a modifier or is already running.
			// Returns the modifier's instance id, or 0.
			uint32_t Start(const std::shared_ptr<Object>& object);

			// Ends it on the object, if it is running; its Deactivate runs as on any end.
			void Stop(const std::shared_ptr<Object>& object);

		private:
			Lua& mLua;
	};
}

#endif
