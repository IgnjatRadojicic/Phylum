
#ifndef _GAME_ABILITYEVENTDELIVERY_HEADER
#define _GAME_ABILITYEVENTDELIVERY_HEADER

// Include
#include "abilityevent.h"

#include <sol/sol.hpp>

// Game
namespace Game {
	class Lua;
	class ModifierSystem;

	// AbilityEventDelivery
	// Hands an event to the agent's scripts as the client does (0x9df330 -> 0x9deca0): first each
	// running cast, then each modifier, if its handledEvents has the type. A handler that answers
	// false ends that cast or modifier; any other answer keeps it.
	class AbilityEventDelivery : public AbilityEventSink {
		public:
			AbilityEventDelivery(Lua& lua, ModifierSystem& modifiers);

			void SendEvent(uint32_t agent, const AbilityEvent& event) override;

		private:
			sol::table ToScript(const AbilityEvent& event) const;
			void SendToCasts(uint32_t agent, const sol::table& data);

		private:
			Lua& mLua;
			ModifierSystem& mModifiers;
	};
}

#endif
