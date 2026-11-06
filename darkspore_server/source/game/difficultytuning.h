
#ifndef _GAME_DIFFICULTYTUNING_HEADER
#define _GAME_DIFFICULTYTUNING_HEADER

// Include
#include <cstdint>
#include <vector>

namespace pugi {
	class xml_node;
}

// Game
namespace Game {
	// DifficultyTuning
	// Values from the game's DifficultyTuning file that change with difficulty. The difficulty
	// is the level number, counted the way the chain counts levels: 1 is 1-1, 5 is 2-1.
	class DifficultyTuning {
		public:
			DifficultyTuning() = default;
			explicit DifficultyTuning(std::vector<float> ratingConversions);

			// Reads the <difficultytuning> root of the game's file.
			static DifficultyTuning FromXml(const pugi::xml_node& root);

			// data/serverdata/difficultytuning, read on first use.
			static const DifficultyTuning& Game();

			// Rating points that make one percent (nTuning.DefenseRatingPerPercent). Outside the
			// table it gives the nearest end, and 0 when the table is empty.
			float GetRatingConversion(uint32_t difficulty) const;

		private:
			std::vector<float> mRatingConversions;
	};
}

#endif
