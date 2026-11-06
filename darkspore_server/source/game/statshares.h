
#ifndef _GAME_STATSHARES_HEADER
#define _GAME_STATSHARES_HEADER

// Include
#include "attributes.h"

#include <array>
#include <optional>

// Game
namespace Game {
	struct MagicNumbers;

	// StatShare
	// One stat that grows with a primary stat: derived = class base + primary * perPoint.
	struct StatShare {
		AttributeType derived;
		AttributeType primary;
		float perPoint;
	};

	// StatShares
	// The client builds a hero's stats this way (Darkspore.exe 0x9e2580), with the per-point
	// values from MagicNumbers:
	//   MaxHealth from Strength, MaxMana from Mind, PhysicalDefense and CriticalRating from
	//   Dexterity, EnergyDefense from Mind.
	class StatShares {
		public:
			static constexpr size_t Count = 5;

			explicit StatShares(const MagicNumbers& numbers);

			const std::array<StatShare, Count>& All() const { return mShares; }

			// The share for a derived stat, if it has one.
			std::optional<StatShare> For(AttributeType derived) const;

			// Whether a stat feeds any derived one.
			bool IsPrimary(AttributeType attribute) const;

			static float Value(const StatShare& share, float classBase, float primaryValue);

		private:
			std::array<StatShare, Count> mShares;
	};
}

#endif
