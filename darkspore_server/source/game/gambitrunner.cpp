
// Include
#include "gambitrunner.h"

#include "castpayment.h"
#include "conditionregistry.h"
#include "objectmanager.h"

#include "utils/functions.h"

// Game
namespace Game {
	namespace {
		// The AI casts at the first rank; a pet's script reads its owner for anything stronger.
		constexpr int32_t aiRank = 1;
	}

	GambitRunner::GambitRunner(Object& object, Lua& lua, BestTargetFn bestTarget) :
		mObject(object), mLua(lua), mBestTarget(std::move(bestTarget)) {}

	std::optional<GambitCast> GambitRunner::Choose(const Phase& phase, uint8_t& cursor) const {
		const auto& gambits = phase.GetGambit();
		if (gambits.empty()) {
			return std::nullopt;
		}

		switch (phase.GetType()) {
			case PhaseType::Sequential: {
				if (cursor >= gambits.size()) {
					cursor = 0;
				}
				auto cast = Try(gambits[cursor]);
				if (cast) {
					cursor = static_cast<uint8_t>((cursor + 1) % gambits.size());
				}
				return cast;
			}

			case PhaseType::Random: {
				const auto index = utils::random::get<uint32_t>(0, static_cast<uint32_t>(gambits.size() - 1));
				return Try(gambits[index]);
			}

			case PhaseType::PrioritizedList:
			default: {
				for (const auto& gambit : gambits) {
					if (auto cast = Try(gambit)) {
						return cast;
					}
				}
				return std::nullopt;
			}
		}
	}

	std::optional<GambitCast> GambitRunner::Try(const GambitDefinition& gambit) const {
		// The cheap checks first: conditions and costs run scripts.
		const auto ability = mLua.GetAbility(gambit.GetAbility());
		if (!ability || mObject.HasCooldown(ability->GetId())) {
			return std::nullopt;
		}

		const auto descriptors = utils::enum_wrapper { ability->GetDescriptors() };
		if (!descriptors.test(Descriptors::IsBasic) && mObject.GetAttributeValue(AttributeType::Silence) != 0) {
			return std::nullopt;
		}

		auto& conditions = mLua.GetConditions();
		const auto result = conditions.Evaluate(gambit.GetCondition(), gambit.GetConditionProperties(), mObject.GetId());
		if (!result.passed) {
			return std::nullopt;
		}

		GambitCast cast { ability };
		if (result.targetId != 0) {
			cast.target = mObject.GetObjectManager().Get(result.targetId);
		} else if (!conditions.Contains(gambit.GetCondition())) {
			// No condition: the gambit fights whatever the AI is fighting.
			cast.target = mBestTarget();
			if (!cast.target) {
				return std::nullopt;
			}
		}

		if (result.position) {
			cast.position = *result.position;
		} else {
			cast.position = cast.target ? cast.target->GetPosition() : mObject.GetPosition();
		}

		if (!CastPayment(mObject, *ability, ability->GetId(), aiRank).CanPay()) {
			return std::nullopt;
		}
		// Out of range it still goes if the agent can walk up to the target first (AI::Pursue); a
		// turret cannot.
		cast.inReach = CanReach(*ability, cast.target);
		// Locomotion, not speed: a pet's speed reads 0 here (its MovementSpeedBuff is 0, as Darkspin
		// sends it too) though the client walks it.
		if (!cast.inReach && !mObject.HasLocomotionData()) {
			return std::nullopt;
		}
		return cast;
	}

	float GambitRunner::ReachDistance(const GambitCast& cast) const {
		// Centre to centre, a little inside the range, so the agent is in reach where it stops.
		constexpr float margin = 0.1f;
		const float range = cast.ability ? cast.ability->GetRange(aiRank) : 0.f;
		const float radii = mObject.GetFootprintRadius() + (cast.target ? cast.target->GetFootprintRadius() : 0.f);
		return std::max(range - margin, 0.f) + radii;
	}

	bool GambitRunner::CanReach(const Ability& ability, const ObjectPtr& target) const {
		if (!target) {
			return true;
		}

		const float range = ability.GetRange(aiRank);
		if (range <= 0) {
			return true;
		}

		// Edge to edge, as nGameObject.GetObjectDistance measures.
		const float gap = glm::distance(mObject.GetPosition(), target->GetPosition()) - mObject.GetFootprintRadius() - target->GetFootprintRadius();
		return gap <= range;
	}
}
