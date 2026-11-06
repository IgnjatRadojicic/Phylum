
#ifndef _GAME_TRIGGERAREA_HEADER
#define _GAME_TRIGGERAREA_HEADER

// Include
#include <glm/glm.hpp>

// Game
namespace Game {
	// TriggerArea
	// Where a script's trigger volume reaches: a sphere (nObjectManager.CreateTriggerVolume,
	// AttachTriggerVolume) or a box (CreateTriggerVolumeBox). The client hands either one to its
	// physics, which reports any body that touches it.
	// Approximate: a body is a sphere of its footprint radius around its position, and the box
	// sizes are half sizes, as physics box shapes take them.
	class TriggerArea {
		public:
			static TriggerArea Sphere(const glm::vec3& center, float radius);

			// halfSize is width (across), length (along facing) and height. A facing with no
			// length on the ground faces +Y, as objects do by default.
			static TriggerArea Box(const glm::vec3& center, const glm::vec3& halfSize, const glm::vec3& facing);

			void MoveTo(const glm::vec3& center);
			const glm::vec3& GetCenter() const;

			// The radius of a sphere around the center that holds the whole shape.
			float GetReach() const;

			bool Touches(const glm::vec3& position, float bodyRadius) const;

		private:
			glm::vec3 mCenter { 0.f };
			glm::vec3 mHalfSize { 0.f };
			glm::vec3 mSide { 1.f, 0.f, 0.f };
			glm::vec3 mForward { 0.f, 1.f, 0.f };
			float mRadius = 0.f;
			bool mIsBox = false;
	};
}

#endif
