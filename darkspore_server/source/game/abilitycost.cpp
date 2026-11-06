
// Include
#include "abilitycost.h"

#include <algorithm>
#include <cmath>

// Game
namespace Game {
	CastCost AbilityCost::For(const AbilityCostData& data, const CasterStats& caster) {
		CastCost cost;
		cost.manual = data.manual;

		if (!caster.overdrive) {
			cost.mana = std::max(0.f, data.manaCost + data.manaCoefficient * caster.attributeValue);
		}

		float seconds = std::max(0.f, data.cooldownSeconds);
		seconds *= std::clamp(1.f - caster.cooldownScale, 0.f, 1.f);
		if (data.basic) {
			seconds /= std::max(-0.9f, caster.attackSpeedScale) + 1.f;
		}
		cost.cooldownMs = static_cast<uint32_t>(std::lround(seconds * 1000.f));

		return cost;
	}
}
