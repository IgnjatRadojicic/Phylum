
// Include
#include "scripttriggervolumes.h"
#include "objectmanager.h"

#include <iterator>
#include <tuple>

// Game
namespace Game {
	// Approximate: room for the widest body, whose footprint reaches past its bounding box.
	constexpr float largestBodyRadius = 15.f;

	ScriptTriggerVolumes::ScriptTriggerVolumes(Call call) : mCall(std::move(call)) {}

	uint32_t ScriptTriggerVolumes::Add(const TriggerArea& shape, ScriptTriggerCallbacks callbacks, uint32_t ownerId) {
		while (mNextHandle == 0 || mVolumes.contains(mNextHandle)) {
			++mNextHandle;
		}

		const uint32_t handle = mNextHandle++;
		mVolumes.emplace(handle, Volume { shape, std::move(callbacks), {}, ownerId });
		return handle;
	}

	bool ScriptTriggerVolumes::Remove(uint32_t volume) {
		return mVolumes.erase(volume) > 0;
	}

	void ScriptTriggerVolumes::Update(const ObjectManager& objects) {
		std::vector<std::tuple<uint32_t, Event, uint32_t>> events;
		for (auto& [handle, volume] : mVolumes) {
			if (volume.ownerId != 0) {
				if (const auto owner = objects.Get(volume.ownerId)) {
					volume.shape.MoveTo(owner->GetPosition());
				}
			}

			const BoundingSphere area(volume.shape.GetCenter(), volume.shape.GetReach() + largestBodyRadius);
			std::vector<uint32_t> inside;
			for (const auto& object : objects.GetObjectsInRadius(area, {})) {
				if (!object->IsTrigger() && volume.shape.Touches(object->GetPosition(), object->GetBodyRadius())) {
					inside.push_back(object->GetId());
				}
			}

			const auto changes = volume.occupancy.Update(inside);
			for (uint32_t id : changes.exited) {
				events.emplace_back(handle, Event::Exit, id);
			}
			for (uint32_t id : changes.entered) {
				events.emplace_back(handle, Event::Enter, id);
			}
			if (volume.callbacks.stay.valid()) {
				for (uint32_t id : changes.stayed) {
					events.emplace_back(handle, Event::Stay, id);
				}
			}
		}

		for (const auto& [handle, event, id] : events) {
			Fire(handle, event, id);
		}
	}

	void ScriptTriggerVolumes::OnObjectRemoved(uint32_t objectId) {
		std::vector<uint32_t> exits;
		for (auto& [handle, volume] : mVolumes) {
			if (volume.occupancy.Remove(objectId)) {
				exits.push_back(handle);
			}
		}
		for (uint32_t handle : exits) {
			Fire(handle, Event::Exit, objectId);
		}

		std::erase_if(mVolumes, [objectId](const auto& entry) { return entry.second.ownerId == objectId; });
	}

	size_t ScriptTriggerVolumes::Count() const {
		return mVolumes.size();
	}

	void ScriptTriggerVolumes::Fire(uint32_t volume, Event event, uint32_t objectId) {
		const auto it = mVolumes.find(volume);
		if (it == mVolumes.end()) {
			return;
		}

		const auto& callbacks = it->second.callbacks;
		// A copy, since the callback may remove its own volume.
		const sol::main_function callback = event == Event::Enter ? callbacks.enter
			: event == Event::Exit ? callbacks.exit
			: callbacks.stay;
		if (callback.valid()) {
			mCall(callback, volume, objectId);
		}
	}
}
