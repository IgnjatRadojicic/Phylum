
// Include
#include "abilitycost.h"

#include <algorithm>
#include <cmath>

// Game
namespace Game {
	CastCost AbilityCost::For(const AbilityCostData& data, const CasterStats& caster, const CooldownRoll& roll) {
		CastCost cost;
		cost.manual = data.manual;

		if (!caster.overdrive) {
			cost.mana = std::max(0.f, data.manaCost + data.manaCoefficient * caster.attributeValue);
		}

		const float base = std::max(0.f, data.cooldownSeconds);
		const float variance = roll.amount * data.cooldownVariance * base;
		float seconds = roll.direction < 0.5f ? base + variance : base - variance;
		if (data.basic) {
			seconds /= std::max(-0.9f, caster.attackSpeedScale) + 1.f;
		} else {
			seconds *= 1.f - caster.cooldownScale;
		}
		seconds = std::max(0.f, seconds);
		cost.cooldownMs = static_cast<uint32_t>(std::lround(seconds * 1000.f));

		return cost;
	}
}
