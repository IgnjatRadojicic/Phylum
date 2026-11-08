
#ifndef _GAME_JUMPARC_HEADER
#define _GAME_JUMPARC_HEADER

// Include
#include <glm/glm.hpp>

#include <optional>

// Game
namespace Game {
	// JumpArc
	// The path of an object that jumps or slides (nLocomotion.JumpInDirection and SlideToPoint):
	// a steady speed along its direction, with a height that rises and falls once. The client
	// plays the same move from the ObjectJump packet, so the server only follows along.
	// Approximate: the client fits the jump to its navigation mesh, which the server does not
	// have, and how it shapes the height is not known. Here the height grows from the minimum to
	// the maximum as the distance reaches rangeToHitMaxHeight, on a parabola.
	class JumpArc {
		public:
			// Gives nothing for a jump with no distance or speed, which the client turns down too.
			static std::optional<JumpArc> Build(const glm::vec3& start, const glm::vec3& direction, float distance, float speed,
				float minHeight, float maxHeight, float rangeToHitMaxHeight);

			glm::vec3 GetPositionAt(float seconds) const;
			// Where the jump is at that time, on its line along the ground (with no height).
			glm::vec3 GetGroundAt(float seconds) const;
			bool IsFinished(float seconds) const;

			const glm::vec3& GetStart() const;
			const glm::vec3& GetDestination() const;
			const glm::vec3& GetDirection() const;
			float GetHeight() const;
			float GetDuration() const;

		private:
			glm::vec3 mStart {};
			glm::vec3 mDestination {};
			glm::vec3 mDirection {};
			float mHeight = 0;
			float mDuration = 0;
	};
}

#endif
