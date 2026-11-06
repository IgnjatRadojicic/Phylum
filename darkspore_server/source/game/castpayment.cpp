
// Include
#include "castpayment.h"

#include "config.h"
#include "lua.h"
#include "object.h"

#include "utils/functions.h"

// Game
namespace Game {
	CastPayment::CastPayment(Object& caster, const Ability& ability, uint32_t abilityId, int32_t rank) :
		mCaster(caster), mAbilityId(abilityId)
	{
		CasterStats stats;
		stats.attributeValue = caster.GetAbilityAttributeValue(ability);
		stats.cooldownScale = caster.GetAttributeValue(AttributeType::CooldownScale);
		stats.attackSpeedScale = caster.GetAttributeValue(AttributeType::AttackSpeedScale);
		stats.overdrive = caster.IsOverdriveCharged();

		CooldownRoll roll;
		roll.amount = utils::random::get<float>(0.f, 1.f);
		roll.direction = utils::random::get<float>(0.f, 1.f);

		mCost = AbilityCost::For(ability.GetCostData(caster.shared_from_this(), rank), stats, roll);
		if (Config::GetBool(ConfigKey::CONFIG_TEST_FREE_MANA)) {
			mCost.mana = 0;
		}
	}

	const CastCost& CastPayment::GetCost() const {
		return mCost;
	}

	bool CastPayment::CanPay() const {
		return !mCaster.HasCooldown(mAbilityId) && mCaster.GetMana() >= mCost.mana;
	}

	Cooldown CastPayment::Pay() {
		if (mCost.mana > 0) {
			mCaster.SetMana(mCaster.GetMana() - mCost.mana);
		}

		if (mCost.cooldownMs == 0) {
			return {};
		}
		return mCaster.AddCooldown(mAbilityId, mCost.cooldownMs);
	}
}
