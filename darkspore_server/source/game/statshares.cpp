
// Include
#include "statshares.h"
#include "magicnumbers.h"

#include <algorithm>

// Game
namespace Game {
	StatShares::StatShares(const MagicNumbers& numbers) : mShares {{
		{ AttributeType::MaxHealth, AttributeType::Strength, numbers.healthPerStrength },
		{ AttributeType::MaxMana, AttributeType::Mind, numbers.manaPerMind },
		{ AttributeType::PhysicalDefense, AttributeType::Dexterity, numbers.physicalDefensePerDexterity },
		{ AttributeType::EnergyDefense, AttributeType::Mind, numbers.energyDefensePerMind },
		{ AttributeType::CriticalRating, AttributeType::Dexterity, numbers.critRatingPerDexterity },
	}} {}

	std::optional<StatShare> StatShares::For(AttributeType derived) const {
		const auto it = std::find_if(mShares.begin(), mShares.end(), [derived](const auto& share) { return share.derived == derived; });
		return it != mShares.end() ? std::optional(*it) : std::nullopt;
	}

	bool StatShares::IsPrimary(AttributeType attribute) const {
		return std::any_of(mShares.begin(), mShares.end(), [attribute](const auto& share) { return share.primary == attribute; });
	}

	float StatShares::Value(const StatShare& share, float classBase, float primaryValue) {
		return classBase + primaryValue * share.perPoint;
	}
}
