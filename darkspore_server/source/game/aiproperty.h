
#ifndef _GAME_AIPROPERTY_HEADER
#define _GAME_AIPROPERTY_HEADER

// Include
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

// Game
namespace Game {
	// AIPropertyType
	// The reflection types a condition's or gambit's property can have (GlobalDefinitions.lua's
	// kReflectionType_*, Darkspore.exe 0xa67430).
	namespace AIPropertyType {
		constexpr uint32_t Bool = 0x68FE5F59;
		constexpr uint32_t Int = 0x1F886EB0;
		constexpr uint32_t Float = 0x4EDCD7A9;
		constexpr uint32_t UInt = 0xE48967A3;
		constexpr uint32_t Guid = 0x2E10AAAE;
		constexpr uint32_t String = 0xF6C8069D;
	}

	// AIProperty
	// A named value a gambit hands its condition (Closest's Radius), or a condition's default for it.
	// The game keeps them as text and works out the value when the asset loads; nCondition.GetProperty
	// gives that value. A type it does not know gives nil.
	struct AIProperty {
		using Value = std::variant<std::monostate, bool, double, std::string>;

		static AIProperty Make(std::string name, uint32_t type, std::string text);

		std::string name;
		std::string text;
		Value value;
		uint32_t nameId = 0;
		uint32_t type = 0;
	};

	using AIProperties = std::vector<AIProperty>;

	// The property with this name id, or null.
	const AIProperty* FindAIProperty(const AIProperties& properties, uint32_t nameId);
}

#endif
