
// Include
#include "abilityeventdelivery.h"

#include "lua.h"
#include "modifiersystem.h"

// Game
namespace Game {
	AbilityEventDelivery::AbilityEventDelivery(Lua& lua, ModifierSystem& modifiers) : mLua(lua), mModifiers(modifiers) {}

	void AbilityEventDelivery::SendEvent(uint32_t agent, const AbilityEvent& event) {
		// Most objects run nothing that could hear it (a pet's every move order raises one).
		if (!mLua.GetRunningCasts().Any(agent) && !mModifiers.HasModifiers(agent)) {
			return;
		}

		const auto data = ToScript(event);
		SendToCasts(agent, data);
		mModifiers.DeliverEvent(agent, data);
	}

	sol::table AbilityEventDelivery::ToScript(const AbilityEvent& event) const {
		sol::state_view state(mLua.GetState());

		sol::table data = state.create_table();
		data["eventType"] = static_cast<uint32_t>(event.GetType());
		// Plain create_table: given a size_t, sol takes the first argument as a global's name.
		sol::table guids = state.create_table();
		sol::table floats = state.create_table();
		sol::table ints = state.create_table();
		for (size_t slot = 1; slot <= AbilityEvent::SlotCount; ++slot) {
			guids[slot] = event.GetGUID(slot);
			floats[slot] = event.GetFloat(slot);
			ints[slot] = event.GetInt(slot);
		}
		data["guids"] = guids;
		data["floats"] = floats;
		data["ints"] = ints;
		return data;
	}

	void AbilityEventDelivery::SendToCasts(uint32_t agent, const sol::table& data) {
		sol::state_view state(mLua.GetState());
		const sol::function handle = state["ModifierApi"]["HandleEvent"];

		const auto& running = mLua.GetRunningCasts();
		for (const auto& cast : running.Of(agent)) {
			// An earlier handler can have ended it, and its thread can carry another cast by now.
			if (!running.IsRunning(cast)) {
				continue;
			}

			// The answer comes back through the context, because the handler runs on a thread of its own.
			sol::table context = mLua.GetCastContext(cast.thread);
			if (!context.valid()) {
				continue;
			}
			mLua.SpawnThread()->call<void>(handle, context, data);

			if (context.get_or("endsByEvent", false) && running.IsRunning(cast)) {
				mLua.Trace("CAST ended by event type=" + std::to_string(data.get<uint32_t>("eventType")) + " agent=" + std::to_string(agent));
				mLua.EndCast(cast.thread);
			}
		}
	}
}
