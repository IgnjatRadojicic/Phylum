
// Include
#include "abilitycost.h"

#include <algorithm>
#include <cmath>

// Game
namespace Game {
	CastCost AbilityCost::For(const AbilityCostData& data, const CasterStats& caster, const CooldownRoll& roll) {
		CastCost cost;
		cost.manual = data.manual;

		// The client adds the attribute share only to a cost that is not 0 (0x9dd7cf): a free
		// ability stays free whatever its coefficient.
		if (!caster.overdrive && data.manaCost != 0.f) {
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
