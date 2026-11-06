
// Include
#include "passivemodifiers.h"

#include "lua.h"
#include "noun.h"
#include "object.h"

// Game
namespace Game {
	PassiveModifiers::PassiveModifiers(Lua& lua) : mLua(lua) {}

	uint32_t PassiveModifiers::NamedBy(const Noun& noun) {
		if (const auto& playerClass = noun.GetPlayerClassData()) {
			return playerClass->GetPassiveModifier();
		}
		if (const auto& aiDefinition = noun.GetAIDefinition()) {
			return aiDefinition->GetPassiveAbility();
		}
		return 0;
	}

	uint32_t PassiveModifiers::Start(const std::shared_ptr<Object>& object) {
		if (!object || !object->GetNoun()) {
			return 0;
		}

		const uint32_t guid = NamedBy(*object->GetNoun());
		if (guid == 0) {
			return 0;
		}

		// The Lua side knows which names are modifiers, and requests it the way scripts do.
		sol::state_view state(mLua.GetState());
		sol::protected_function start = state["ModifierApi"]["StartPassive"];
		sol::protected_function_result result = start(object->GetId(), guid);
		if (!result.valid()) {
			sol::error error = result;
			mLua.Trace(std::string("ERROR passive guid=") + std::to_string(guid) + " " + error.what());
			return 0;
		}
		return result.get<uint32_t>();
	}
}
