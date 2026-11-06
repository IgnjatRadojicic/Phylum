
#ifndef _GAME_CASTPAYMENT_HEADER
#define _GAME_CASTPAYMENT_HEADER

// Include
#include "abilitycost.h"
#include "object.h"

#include <cstdint>

// Game
namespace Game {
	// CastPayment
	// One cast's mana and cooldown, for one caster: whether it can pay now, and paying it.
	// The cost comes from the ability's script (AbilityCost has the rules). A manual ability
	// is paid later, when its script calls nAbility.PayCooldownAndMana.
	class CastPayment {
		public:
			CastPayment(Object& caster, const Ability& ability, uint32_t abilityId, int32_t rank);

			const CastCost& GetCost() const;

			// No cooldown running and enough mana.
			bool CanPay() const;

			// Takes the mana and starts the cooldown. Returns the cooldown now running, which is
			// empty when the cast has none. With TEST_FREE_MANA, until orbs refill mana as in the
			// game, a cast costs no mana at all.
			Cooldown Pay();

		private:
			Object& mCaster;
			uint32_t mAbilityId;
			CastCost mCost;
	};
}

#endif
