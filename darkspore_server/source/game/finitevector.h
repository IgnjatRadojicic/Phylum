#ifndef _GAME_FINITEVECTOR_HEADER
#define _GAME_FINITEVECTOR_HEADER

// Include
#include <glm/glm.hpp>

#include <cmath>

// Game
namespace Game {
	// A position or direction that a script worked out can come out NaN or infinite, when the game's
	// own Vector.normalize divides by a zero length. The client crashes on such a position: it reads
	// past an empty list at 0xa46b10, about half a minute after a jump to it. So nothing like that
	// may move an object.
	inline bool IsFinite(const glm::vec3& value) {
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}
}

#endif
