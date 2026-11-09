
#ifndef _GAME_WALKTIMER_HEADER
#define _GAME_WALKTIMER_HEADER

// Include
#include <glm/glm.hpp>

// Game
namespace Game {
	// The server puts a walking object on its goal at once, while the client walks it there at its
	// move speed. This times that walk, so the server knows how far the client still has to go and
	// where on its way the client has it now. Straight across the ground; the client's path may bend.
	class WalkTimer {
		public:
			// No speed means the walk cannot be timed, so it counts as there. Started while a walk is
			// under way, it starts from where the client is on that one, not from `from`.
			void Start(const glm::vec3& from, const glm::vec3& to, float speed);
			void Advance(float seconds);
			void Clear();

			// Ground distance the client still has to walk.
			float GetDistanceLeft() const;
			bool IsWalking() const;

			// Where the client has the object now: on the line from the start to the goal.
			glm::vec3 GetPosition() const;

		private:
			glm::vec3 mFrom {};
			glm::vec3 mTo {};
			float mLength = 0;
			float mDistanceLeft = 0;
			float mSpeed = 0;
	};

	// Where a walk from `from` to `goal` ends when it stops `stop` short of the goal (as the client
	// stops a walk at its stop distance). Within the distance already, it stays where it is.
	glm::vec3 StopShort(const glm::vec3& from, const glm::vec3& goal, float stop);
}

#endif
