
// Include
#include "herodeploy.h"

#include "lua.h"
#include "object.h"
#include "serverevent.h"

#include "utils/functions.h"

#include <string>

// Game
namespace Game {
	HeroDeploy::HeroDeploy(Lua& lua) : mLua(lua) {}

	uint32_t HeroDeploy::Start(const std::shared_ptr<Object>& hero, Arrival arrival) {
		if (!hero) {
			return 0;
		}

		sol::state_view state(mLua.GetState());
		sol::protected_function deploy = state["ModifierApi"]["Deploy"];
		sol::protected_function_result result = deploy(hero->GetId(), static_cast<int32_t>(arrival));
		if (!result.valid()) {
			sol::error error = result;
			mLua.Trace(std::string("ERROR deploy agent=") + std::to_string(hero->GetId()) + " " + error.what());
			return 0;
		}
		return result.get<uint32_t>();
	}

	ServerEvent HeroDeploy::BeamOut(const Object& hero) {
		const auto name = std::string("character_beam_out_") + std::string(Element(hero.GetCreatureType())) + ".ServerEventDef";

		ServerEvent serverEvent;
		serverEvent.mServerEventDef = utils::hash_id(name);
		serverEvent.mPosition = hero.GetPosition();
		return serverEvent;
	}

	std::string_view HeroDeploy::Element(uint32_t creatureType) {
		switch (creatureType) {
			case 0: return "cyber";        // Technology
			case 1: return "spacetime";
			case 2: return "bio";          // Life
			case 3: return "plasma_flame"; // Elements
			default: return "necro";
		}
	}
}
