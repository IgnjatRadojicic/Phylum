
// Include
#include "modifiersystem.h"

#include "actionevents.h"
#include "instance.h"
#include "lua.h"
#include "object.h"
#include "objectmanager.h"

#include <algorithm>

// Game
namespace Game {
	ModifierSystem::ModifierSystem(Instance& game) : mGame(game) {}

	uint32_t ModifierSystem::Request(const ModifierSpec& spec, const ModifierDurationRules& rules, float seconds, uint32_t agent, uint32_t initiator, sol::table context) {
		using Action = ModifierDecision::Action;

		// A deactivate script can ask for a modifier on an agent that is on its way out.
		const auto object = mGame.GetObjectManager().Get(agent);
		if (!object || object->IsMarkedForDeletion()) {
			return 0;
		}

		const auto duration = ResolveModifierDuration(seconds, rules);
		if (!duration) {
			Trace("resisted", spec.guid, 0, agent, initiator);
			return 0;
		}

		auto resolved = spec;
		resolved.durationMs = *duration;

		const auto decision = mManager.Request(resolved, agent, initiator, mGame.GetTime());
		switch (decision.action) {
			case Action::Refused:
				Trace("refused", spec.guid, decision.id, agent, initiator);
				return 0;

			case Action::Refreshed:
				Trace("refreshed", spec.guid, decision.id, agent, initiator);
				SendUpdated(decision.id, true);
				return decision.id;

			case Action::Stacked:
				Trace("stacked", spec.guid, decision.id, agent, initiator);
				Stack(decision.id);
				return decision.id;

			case Action::Replaced:
				Finish(decision.replacedId);
				[[fallthrough]];

			case Action::Created:
				Trace("started", spec.guid, decision.id, agent, initiator);
				Start(decision.id, std::move(context));
				return decision.id;
		}
		return 0;
	}

	void ModifierSystem::Update() {
		for (uint32_t id : mManager.GetFinished(mGame.GetTime())) {
			Finish(id);
		}
	}

	void ModifierSystem::OnAgentDeath(uint32_t agent) {
		for (uint32_t id : mManager.GetAll(agent)) {
			if (mManager.Get(id)->spec.deactivation == DeactivationType::OnAgentDeath) {
				mManager.MarkForDelete(id);
			}
		}
	}

	void ModifierSystem::OnAgentDestroyed(uint32_t agent) {
		for (uint32_t id : mManager.GetAll(agent)) {
			Finish(id);
		}
	}

	void ModifierSystem::MarkForDelete(uint32_t id) {
		mManager.MarkForDelete(id);
	}

	bool ModifierSystem::AdoptThread(uint32_t id, uint64_t ticket) {
		const auto it = mRuns.find(id);
		if (it == mRuns.end() || ticket == 0) {
			return false;
		}
		it->second.children.push_back(ticket);
		return true;
	}

	void ModifierSystem::ResetDuration(uint32_t id) {
		mManager.ResetDuration(id, mGame.GetTime());
		SendUpdated(id, true);
	}

	void ModifierSystem::SetStackCount(uint32_t id, uint32_t count) {
		mManager.SetStackCount(id, count);
		SendUpdated(id, false);
	}

	uint32_t ModifierSystem::AddAttributeChange(uint32_t object, uint8_t attribute, float value, AttributeOp op, uint32_t id) {
		const auto target = mGame.GetObjectManager().Get(object);
		if (!target) {
			return 0;
		}

		const auto handle = target->AddAttributeChange(attribute, value, op);

		// A modifier takes its changes with it when it ends, even if its script never does.
		if (auto it = mRuns.find(id); it != mRuns.end()) {
			it->second.changes.push_back({ target, handle });
		}
		return handle;
	}

	void ModifierSystem::RemoveAttributeChange(uint32_t object, uint32_t handle, uint32_t id) {
		const auto target = mGame.GetObjectManager().Get(object);
		if (!target) {
			return;
		}

		target->RemoveAttributeChange(handle);

		if (auto it = mRuns.find(id); it != mRuns.end()) {
			std::erase_if(it->second.changes, [&target, handle](const auto& change) {
				return change.object.lock() == target && change.handle == handle;
			});
		}
	}

	const ModifierManager& ModifierSystem::GetManager() const {
		return mManager;
	}

	sol::object ModifierSystem::GetContext(uint32_t id, sol::this_state L) const {
		if (const auto it = mRuns.find(id); it != mRuns.end()) {
			return sol::make_object(L, it->second.context);
		}
		return sol::make_object(L, sol::lua_nil);
	}

	void ModifierSystem::Start(uint32_t id, sol::table context) {
		// The modifier this one replaced can end it from its deactivate script.
		const auto instance = mManager.Get(id);
		if (!instance) {
			return;
		}

		// Abilities have instance numbers too, and they can equal this id.
		context["instanceId"] = id;
		context["modifierId"] = id;
		mRuns[id].context = sol::main_table(context);

		if (const auto object = mGame.GetObjectManager().Get(instance->agent)) {
			mGame.SendModifierCreated(object, *instance);
		}

		// The thread stays with the modifier, because most ticks wait until they are stopped.
		auto& lua = mGame.GetLua();
		auto thread = lua.SpawnThread();

		mRuns[id].ticket = lua.TrackThread(thread);

		// Start waits one update before Tick (as the client does), so a Tick that dies before it
		// first waits fails on the next resume; that still counts as dying at start.
		thread->on_next_resume_failure([this, id] {
			if (const auto running = mManager.Get(id)) {
				Trace("failed", running->spec.guid, id, running->agent, running->initiator);
				mManager.MarkForDelete(id);
			}
		});

		sol::state_view state(lua.GetState());
		const bool started = thread->call<void>(state["ModifierApi"]["Start"], context);

		// A script that dies at start would otherwise hold the modifier, doing nothing, for its
		// whole duration. It ends at the next update, so deactivate can undo what activate did.
		// Only the start is covered: a script that fails later, after waiting, keeps its duration.
		// Looked up again, because the script may already have ended it.
		if (const auto running = mManager.Get(id); running && !started) {
			Trace("failed", running->spec.guid, id, running->agent, running->initiator);
			mManager.MarkForDelete(id);
		}
	}

	void ModifierSystem::Stack(uint32_t id) {
		const auto it = mRuns.find(id);
		if (it == mRuns.end()) {
			return;
		}

		// A script that handles the event counts for itself, and most keep to the limit that way.
		if (!CallScript("Stack", sol::table(it->second.context))) {
			const auto instance = mManager.Get(id);
			mManager.SetStackCount(id, instance->stackCount + 1);
			ResetDuration(id);
		}
	}

	void ModifierSystem::Finish(uint32_t id) {
		const auto it = mRuns.find(id);
		if (it == mRuns.end()) {
			mManager.Remove(id);
			return;
		}

		// A deactivate script that asks for this modifier again must not find the one that is ending.
		mManager.MarkForDelete(id);

		// Moved out first: deactivate scripts can request and end other modifiers.
		auto run = std::move(it->second);
		mRuns.erase(it);

		// Threads that have already finished have given their tickets back, so stopping them does nothing.
		auto& lua = mGame.GetLua();
		lua.StopThread(run.ticket);
		for (const auto child : run.children) {
			lua.StopThread(child);
		}

		const auto instance = mManager.Get(id);
		const auto agent = instance ? instance->agent : 0;
		const auto guid = instance ? instance->spec.guid : 0;
		Trace("ended", guid, id, agent, instance ? instance->initiator : 0);

		// Kept while deactivating, so the script finds its context and can take off its own changes.
		// Anything it adds then ends with it too, though no game script does that.
		auto& ending = mRuns[id];
		ending.context = run.context;
		ending.changes = std::move(run.changes);
		CallScript("Stop", sol::table(run.context));

		for (const auto& change : mRuns[id].changes) {
			if (const auto object = change.object.lock()) {
				object->RemoveAttributeChange(change.handle);
			}
		}
		mRuns.erase(id);

		const auto object = mGame.GetObjectManager().Get(agent);
		if (object) {
			mGame.SendModifierDeleted(object, id);
		}
		mManager.Remove(id);

		// Last, as in the client (0x9e11f0 raises it at 0x9e1261 after the modifier has left the
		// agent), and only when no other modifier of that GUID is left. An agent on its way out
		// hears nothing: its other modifiers are ending with it.
		if (object && !object->IsMarkedForDeletion() && !mManager.FindFirst(agent, guid)) {
			if (auto events = mGame.GetActionEvents()) {
				events->LostModifierOfType(agent, guid);
			}
		}
	}

	void ModifierSystem::DeliverEvent(uint32_t agent, const sol::table& data) {
		// A copy of the ids, because handlers can start and end modifiers.
		for (const uint32_t id : mManager.GetAll(agent)) {
			const auto it = mRuns.find(id);
			if (it == mRuns.end()) {
				continue;
			}

			sol::table context(it->second.context);
			CallScript("HandleEvent", context, data);
			if (context.get_or("endsByEvent", false)) {
				EndByEvent(id);
			}
		}
	}

	bool ModifierSystem::HasModifiers(uint32_t agent) const {
		return !mManager.GetAll(agent).empty();
	}

	void ModifierSystem::EndByEvent(uint32_t id) {
		const auto it = mRuns.find(id);
		if (it == mRuns.end()) {
			return;
		}

		if (const auto instance = mManager.Get(id)) {
			Trace("ending-on-event", instance->spec.guid, id, instance->agent, instance->initiator);
		}

		// A thread cannot be stopped while it runs, so a modifier that sent the event to its own
		// agent ends at the next update instead, as the client defers an instance it is inside
		// (0x9dea20 sets +0x165). Otherwise it ends now (0x9e11f0).
		const auto& lua = mGame.GetLua();
		bool running = lua.IsThreadExecuting(it->second.ticket);
		for (const auto child : it->second.children) {
			running = running || lua.IsThreadExecuting(child);
		}

		if (running) {
			mManager.MarkForDelete(id);
		} else {
			Finish(id);
		}
	}

	bool ModifierSystem::CallScript(const char* function, sol::table context, sol::object argument) {
		auto& lua = mGame.GetLua();

		sol::state_view state(lua.GetState());
		sol::function script = state["ModifierApi"][function];

		// The result travels through the context, because the thread may yield or go back to the pool.
		context["handled"] = false;
		lua.SpawnThread()->call<void>(script, context, argument);
		return context.get_or("handled", false);
	}

	void ModifierSystem::Trace(const char* what, uint32_t guid, uint32_t id, uint32_t agent, uint32_t initiator) {
		mGame.GetLua().Trace(std::string("MODIFIER ") + what + " guid=" + std::to_string(guid) + " id=" + std::to_string(id) +
			" agent=" + std::to_string(agent) + " initiator=" + std::to_string(initiator));
	}

	void ModifierSystem::SendUpdated(uint32_t id, bool restarted) {
		const auto instance = mManager.Get(id);
		if (!instance) {
			return;
		}

		if (const auto object = mGame.GetObjectManager().Get(instance->agent)) {
			mGame.SendModifierUpdated(object, *instance, restarted);
		}
	}
}
