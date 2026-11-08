
#ifndef _GAME_FACING_HEADER
#define _GAME_FACING_HEADER

// Include
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <optional>

// Game
namespace Game {
	// The orientation that looks along a direction on the ground: a turn about +Z, taking +Y as
	// forward (as nObjectManager.CreateCreature does). Height is ignored. Nothing when the direction
	// has no length across the ground (or is not a number), so the caller keeps the orientation it has.
	// glm::quat(direction) would read the direction as Euler angles instead.
	std::optional<glm::quat> OrientationFromDirection(const glm::vec3& direction);

	// How far apart two orientations are, in radians from 0 to pi.
	float AngleBetween(const glm::quat& from, const glm::quat& to);
}

#endif
