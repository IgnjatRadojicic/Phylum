
#ifndef _GAME_PROJECTILESWEEP_HEADER
#define _GAME_PROJECTILESWEEP_HEADER

// Include
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// Game
namespace Game {
	// SweepTarget
	// A box around a mob. The minimum and maximum are relative to its position.
	struct SweepTarget {
		uint32_t id = 0;
		glm::vec3 position {};
		glm::vec3 minimum {};
		glm::vec3 maximum {};
	};

	// SweepHit
	struct SweepHit {
		uint32_t id = 0;
		float distance = 0;
		glm::vec3 position {};
	};

	// ProjectileSweep
	// Finds what a projectile's box touches along one tick of movement, in the order it touches
	// them. Testing the whole path means a fast shot cannot jump over a target between ticks. Mobs
	// at the same point always come back in the same order: the intended target, then lowest id.
	class ProjectileSweep {
		public:
			ProjectileSweep(const glm::vec3& from, const glm::vec3& to, const glm::vec3& halfExtent, uint32_t intendedId);

			std::vector<SweepHit> Find(const std::vector<SweepTarget>& targets) const;

		private:
			glm::vec3 mFrom;
			glm::vec3 mTo;
			glm::vec3 mHalfExtent;
			uint32_t mIntendedId;
	};
}

#endif
