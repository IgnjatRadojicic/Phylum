
#ifndef _GAME_LOBARC_HEADER
#define _GAME_LOBARC_HEADER

// Include
#include <glm/glm.hpp>

#include <optional>

// Game
namespace Game {
	// LobArc
	// The path of a lobbed projectile: steady speed along the ground, with height as a curve of
	// the distance covered. The client flies the same curve from these values.
	class LobArc {
		public:
			// Uses flightTime when it is above zero, and speed otherwise. Gives nothing when
			// there is no ground to cover or no way to work out the time.
			static std::optional<LobArc> Build(const glm::vec3& start, const glm::vec3& destination, float height, float flightTime, float speed);

			glm::vec3 GetPositionAt(float seconds) const;
			glm::vec3 GetInitialVelocity() const;

		public:
			glm::vec3 mStart {};
			glm::vec3 mDestination {};
			glm::vec3 mPlaneDirection {};

			float mHeight = 0;
			float mDuration = 0;
			float mPlaneSpeed = 0;
			float mUpLinear = 0;
			float mUpQuadratic = 0;
	};
}

#endif
