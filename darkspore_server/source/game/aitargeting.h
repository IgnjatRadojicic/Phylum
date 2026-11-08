
#ifndef _GAME_AITARGETING_HEADER
#define _GAME_AITARGETING_HEADER

// Include
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// Game
namespace Game {
	// TargetCandidate
	// A creature the AI could fight, as it is at this moment.
	struct TargetCandidate {
		uint32_t id = 0;
		glm::vec3 position {};
		uint8_t team = 0;
		bool alive = true;
		bool targetable = true;
		bool stealthed = false;
	};

	// The AI's targets, best first: live, targetable candidates on another team within the radius,
	// nearest first, a tie going to the lower id. A stealthed one only counts for an agent that
	// detects stealth. The client's nAgent.GetBestTarget takes the first entry of its aggro list
	// that passes the same checks (Darkspore.exe 0x9e3e50); the server keeps no aggro list, so the
	// hostiles in aggro range stand in for it. The AI asks again every time it thinks, so a target
	// that died or went away is dropped.
	std::vector<uint32_t> OrderHostiles(const glm::vec3& from, uint8_t team, float radius, bool detectsStealth,
		const std::vector<TargetCandidate>& candidates, uint32_t selfId);

	// The first of OrderHostiles, or 0 when there is none.
	uint32_t PickNearestHostile(const glm::vec3& from, uint8_t team, float radius, const std::vector<TargetCandidate>& candidates, uint32_t selfId);
}

#endif
