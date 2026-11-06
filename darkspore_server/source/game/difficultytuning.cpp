
// Include
#include "difficultytuning.h"

#include <pugixml.hpp>

#include <algorithm>
#include <iostream>

// Game
namespace Game {
	DifficultyTuning::DifficultyTuning(std::vector<float> ratingConversions) : mRatingConversions(std::move(ratingConversions)) {}

	DifficultyTuning DifficultyTuning::FromXml(const pugi::xml_node& root) {
		// Each value sits one level deeper: <entry><entry>10.0</entry></entry>
		std::vector<float> ratingConversions;
		for (const auto& entry : root.child("RatingConversion").children("entry")) {
			ratingConversions.push_back(entry.child("entry").text().as_float());
		}
		return DifficultyTuning(std::move(ratingConversions));
	}

	const DifficultyTuning& DifficultyTuning::Game() {
		static const DifficultyTuning tuning = [] {
			constexpr const char* path = "data/serverdata/difficultytuning/DifficultyTuning.DifficultyTuning.xml";

			pugi::xml_document document;
			if (!document.load_file(path)) {
				std::cout << "Cannot read " << path << std::endl;
				return DifficultyTuning();
			}
			return FromXml(document.child("difficultytuning"));
		}();
		return tuning;
	}

	float DifficultyTuning::GetRatingConversion(uint32_t difficulty) const {
		if (mRatingConversions.empty()) {
			return 0.f;
		}

		const size_t last = mRatingConversions.size() - 1;
		const size_t index = std::min<size_t>(difficulty > 0 ? difficulty - 1 : 0, last);
		return mRatingConversions[index];
	}
}
