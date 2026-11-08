
// Include
#include "projectilepasslist.h"

// Game
namespace Game {
	ProjectilePassList::Result ProjectilePassList::Check(uint32_t target, float incorporeal, float intangible, float roll) {
		for (const auto id : mIds) {
			if (id == target) {
				return Result::Passes;
			}
		}

		const bool passes = (incorporeal > 0 && roll < incorporeal) || intangible != 0;
		if (!passes) {
			return Result::Hits;
		}

		for (auto& id : mIds) {
			if (id == 0) {
				id = target;
				return Result::PassesFirstTime;
			}
		}
		return Result::Passes;
	}
}
