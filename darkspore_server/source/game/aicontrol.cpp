
// Include
#include "aicontrol.h"

#include "config.h"
#include "object.h"

// Game
namespace Game {
	namespace {
		// Owners chain no deeper than a turret's shot; a loop of owners ends here too.
		constexpr int maxOwnerDepth = 4;
	}

	bool IsHero(const Object& object) {
		return object.IsPlayerControlled() || (object.GetNoun() && object.GetNoun()->GetPlayerClassData());
	}

	bool IsHeroSummon(const Object& object) {
		// A removed owner can outlive its removal while a script still holds it, and the scripts
		// then find no owner: the summon is left to itself, as a mob is.
		auto owner = object.GetOwnerObject();
		for (int depth = 0; owner && owner.get() != &object && !owner->IsMarkedForDeletion() && depth < maxOwnerDepth; ++depth) {
			if (IsHero(*owner)) {
				return true;
			}
			owner = owner->GetOwnerObject();
		}
		return false;
	}

	bool RunsGambits(const Object& object) {
		// Asked on every update for every NPC, and the config is loaded before any game starts.
		static const bool mobs = Config::GetBool(CONFIG_AI_MOBS);
		return mobs || IsHeroSummon(object);
	}
}
