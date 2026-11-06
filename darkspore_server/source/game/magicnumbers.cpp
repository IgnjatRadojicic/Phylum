
// Include
#include "magicnumbers.h"

#include <pugixml.hpp>

#include <iostream>

// Game
namespace Game {
	MagicNumbers MagicNumbers::FromXml(const pugi::xml_node& root) {
		MagicNumbers numbers;
		auto read = [&root](const char* name, float& value) {
			if (const auto node = root.child(name)) {
				value = node.text().as_float(value);
			}
		};
		read("DodgePercentCap", numbers.dodgePercentCap);
		read("ResistPercentCap", numbers.resistPercentCap);
		read("CriticalDamageBonus", numbers.criticalDamageBonus);
		return numbers;
	}

	const MagicNumbers& MagicNumbers::Game() {
		static const MagicNumbers numbers = [] {
			constexpr const char* path = "data/serverdata/magicnumbers/MagicNumbers.MagicNumbers.xml";

			pugi::xml_document document;
			if (!document.load_file(path)) {
				std::cout << "Cannot read " << path << std::endl;
				return MagicNumbers();
			}
			return FromXml(document.child("magicnumbers"));
		}();
		return numbers;
	}
}
