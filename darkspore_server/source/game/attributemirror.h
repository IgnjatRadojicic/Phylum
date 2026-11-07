
#ifndef _GAME_ATTRIBUTE_MIRROR_HEADER
#define _GAME_ATTRIBUTE_MIRROR_HEADER

// Include
#include "attributes.h"
#include "noun.h"

#include <array>

// Game
namespace Game {
	// AttributeMirror
	// nGameObject.MirrorBaseAttributes(pet, source): a summon's class values are shares of its
	// source's stats (client 0xa03020 -> 0x9e2b40). Eight stats are the class share times the
	// source's value before its buffs; max health also grows with the source's PetHealth, buffs
	// included. The client links them and works them out on each read; a source's unbuffed stats do
	// not change during a level, so working them out once on linking gives the same numbers.
	// Not copied (not decoded): the class caps scaled by an unknown factor (attrs+0x610).
	class AttributeMirror {
		public:
			struct Stat {
				AttributeType type;
				ClassAttribute::Key share;
			};

			// 0x9e2120: Strength, Dexterity, Mind, MaxHealth, PhysicalDefense, EnergyDefense,
			// CriticalRating and MovementSpeedBuff.
			static constexpr std::array<Stat, 8> stats {{
				{ AttributeType::Strength, ClassAttribute::Strength },
				{ AttributeType::Dexterity, ClassAttribute::Dexterity },
				{ AttributeType::Mind, ClassAttribute::Mind },
				{ AttributeType::MaxHealth, ClassAttribute::Health },
				{ AttributeType::PhysicalDefense, ClassAttribute::PhysicalDefense },
				{ AttributeType::EnergyDefense, ClassAttribute::EnergyDefense },
				{ AttributeType::CriticalRating, ClassAttribute::Critical },
				{ AttributeType::MovementSpeedBuff, ClassAttribute::MovementSpeedBuff },
			}};

			static float Value(const Stat& stat, float share, float sourceUnbuffed, float sourcePetHealth);

			// Keeps the same part of max health: a pet at full health stays full.
			static float Health(float health, float oldMaxHealth, float newMaxHealth);
	};
}

#endif
