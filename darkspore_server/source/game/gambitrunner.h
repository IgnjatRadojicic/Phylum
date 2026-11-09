
#ifndef _GAME_GAMBITRUNNER_HEADER
#define _GAME_GAMBITRUNNER_HEADER

// Include
#include "object.h"

#include <functional>
#include <optional>

// Game
namespace Game {
	// GambitCast
	// What the AI casts, at what and where.
	struct GambitCast {
		AbilityPtr ability;
		ObjectPtr target;
		glm::vec3 position {};
		// Within the ability's range of the target now; if not, the agent walks there first.
		bool inReach = true;
	};

	// GambitRunner
	// Picks what an AI phase does now. A prioritized list (every phase in the game's data) takes the
	// first gambit that can go: its ability is ready and paid for, its condition holds, and the
	// target is within the ability's range or the agent can walk there (a turret cannot). A condition gives its own
	// target (Closest, SpriteHealTargeting); a gambit without one (TurretTrap) needs the AI's best
	// target, or does nothing. A sequential phase tries only its next gambit, a random one any one.
	class GambitRunner {
		public:
			using BestTargetFn = std::function<ObjectPtr()>;

			GambitRunner(Object& object, Lua& lua, BestTargetFn bestTarget);

			// cursor keeps a sequential phase's place from one call to the next.
			std::optional<GambitCast> Choose(const Phase& phase, uint8_t& cursor) const;

			// The cast's range at the AI's rank (edge to edge).
			float Range(const GambitCast& cast) const;

			// How near (centre to centre) the agent must come to be in the cast's reach.
			float ReachDistance(const GambitCast& cast) const;

		private:
			std::optional<GambitCast> Try(const GambitDefinition& gambit) const;
			bool CanReach(const Ability& ability, const ObjectPtr& target) const;

		private:
			Object& mObject;
			Lua& mLua;
			BestTargetFn mBestTarget;
	};
}

#endif
