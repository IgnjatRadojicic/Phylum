
#ifndef _GAME_SCRIPTTRIGGERVOLUMES_HEADER
#define _GAME_SCRIPTTRIGGERVOLUMES_HEADER

// Include
#include "triggeroccupancy.h"
#include "triggerarea.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <functional>
#include <map>

// Game
namespace Game {
	class ObjectManager;

	// ScriptTriggerCallbacks
	// The Lua functions a script gives a volume. Any of them may be missing. They are kept by the
	// main state, since the thread that gave them goes back to its pool.
	struct ScriptTriggerCallbacks {
		sol::main_function enter;
		sol::main_function exit;
		sol::main_function stay;
	};

	// ScriptTriggerVolumes
	// The trigger volumes scripts make with nObjectManager. In the client these are physics
	// phantoms, not game objects (Darkspore.exe 0xa16d00), so they are never sent to a client.
	// Each tick a volume finds the objects that touch it and calls its callbacks with its handle
	// and the object's id. A volume attached to an object moves with it.
	// From the client: any object counts, the callbacks' results are not used (they only matter
	// for a level's once-only triggers), destroying a volume calls nothing, and an object that
	// leaves the world first exits every volume it is in (0xa270a0).
	// Approximate: a volume's owner counts like any other object, and a volume goes when its
	// owner does.
	class ScriptTriggerVolumes {
		public:
			using Call = std::function<void(const sol::main_function& callback, uint32_t volume, uint32_t objectId)>;

			explicit ScriptTriggerVolumes(Call call);

			// ownerId 0 makes a volume that stays where it is put. Returns its handle, never 0.
			uint32_t Add(const TriggerArea& shape, ScriptTriggerCallbacks callbacks, uint32_t ownerId = 0);
			bool Remove(uint32_t volume);

			// After the objects have moved this tick.
			void Update(const ObjectManager& objects);

			// Before an object leaves the world.
			void OnObjectRemoved(uint32_t objectId);

			size_t Count() const;

		private:
			enum class Event { Enter, Exit, Stay };

			struct Volume {
				TriggerArea shape;
				ScriptTriggerCallbacks callbacks;
				TriggerOccupancy occupancy;
				uint32_t ownerId = 0;
			};

			// A callback can add or remove volumes, so each one is found again by its handle.
			void Fire(uint32_t volume, Event event, uint32_t objectId);

		private:
			std::map<uint32_t, Volume> mVolumes;
			Call mCall;
			uint32_t mNextHandle = 1;
	};
}

#endif
