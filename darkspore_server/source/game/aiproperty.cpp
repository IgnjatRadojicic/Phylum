
// Include
#include "aiproperty.h"

#include "utils/functions.h"

#include <cstdlib>

// Game
namespace Game {
	namespace {
		// The digits at the start, as the game reads them: a sign, then decimal digits up to the first
		// other character. It wraps as a 32-bit number does.
		uint32_t DecimalBits(std::string_view text) {
			while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n')) {
				text.remove_prefix(1);
			}
			const bool negative = !text.empty() && text.front() == '-';
			if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
				text.remove_prefix(1);
			}
			uint32_t bits = 0;
			for (const char c : text) {
				if (c < '0' || c > '9') {
					break;
				}
				bits = bits * 10 + static_cast<uint32_t>(c - '0');
			}
			return negative ? 0u - bits : bits;
		}

		uint32_t UnsignedBits(const std::string& text) {
			if (text.starts_with("0x") || text.starts_with("0X")) {
				return static_cast<uint32_t>(std::strtoul(text.c_str() + 2, nullptr, 16));
			}
			return DecimalBits(text);
		}

		AIProperty::Value Convert(uint32_t type, const std::string& text) {
			switch (type) {
				case AIPropertyType::Bool:
					return text == "true";
				case AIPropertyType::Int:
					return static_cast<double>(static_cast<int32_t>(DecimalBits(text)));
				case AIPropertyType::Float:
					// The game keeps a float, so 0.1 reads back as the float nearest it.
					return static_cast<double>(std::strtof(text.c_str(), nullptr));
				case AIPropertyType::UInt:
				case AIPropertyType::Guid:
					return static_cast<double>(UnsignedBits(text));
				case AIPropertyType::String:
					return text;
				default:
					return std::monostate {};
			}
		}
	}

	AIProperty AIProperty::Make(std::string name, uint32_t type, std::string text) {
		AIProperty property;
		property.nameId = utils::hash_id(name);
		property.type = type;
		property.value = Convert(type, text);
		property.name = std::move(name);
		property.text = std::move(text);
		return property;
	}

	const AIProperty* FindAIProperty(const AIProperties& properties, uint32_t nameId) {
		for (const auto& property : properties) {
			if (property.nameId == nameId) {
				return &property;
			}
		}
		return nullptr;
	}
}
