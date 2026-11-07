
// Include
#include "attributemirror.h"

// Game
namespace Game {
	// AttributeMirror
	float AttributeMirror::Value(const Stat& stat, float share, float sourceUnbuffed, float sourcePetHealth) {
		float value = share * sourceUnbuffed;
		if (stat.type == AttributeType::MaxHealth) {
			value *= 1.f + sourcePetHealth;
		}
		return value;
	}

	float AttributeMirror::Health(float health, float oldMaxHealth, float newMaxHealth) {
		if (oldMaxHealth <= 0.f) {
			return newMaxHealth;
		}
		return newMaxHealth * (health / oldMaxHealth);
	}
}
