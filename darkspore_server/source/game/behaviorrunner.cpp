
// Include
#include "behaviorrunner.h"
#include "instance.h"
#include "lua.h"
#include "object.h"

#include <format>

// Game
namespace Game {
	BehaviorRunner::BehaviorRunner(Object& agent) : mAgent(agent) {}

	void BehaviorRunner::Run(const std::string& name) {
		auto& lua = mAgent.GetGame().GetLua();
		if (name == mName && (mMissing || lua.IsThreadRunning(mTicket))) {
			return;
		}

		// A behavior whose update ended is left and started again, as the client's selector does.
		Exit();
		mMissing = false;
		if (name.empty()) {
			return;
		}

		mName = name;
		const bool defined = sol::state_view(lua.GetState())[name].get_type() == sol::type::table;
		lua.Trace(std::format("BEHAVIOR object={} start={}{}", mAgent.GetId(), name, defined ? "" : " missing"));
		if (!defined) {
			// Traced once; trying again every think would only repeat the line.
			mMissing = true;
			return;
		}
		if (Call(name, 1, false)) {
			Call(name, 2, true);
		}
	}

	void BehaviorRunner::Exit() {
		if (mName.empty()) {
			return;
		}

		mAgent.GetGame().GetLua().StopThread(mTicket);
		mTicket = 0;

		const auto name = std::move(mName);
		mName.clear();
		Call(name, 3, false);
	}

	bool BehaviorRunner::Call(const std::string& name, int step, bool keep) {
		auto& lua = mAgent.GetGame().GetLua();
		sol::state_view state(lua.GetState());

		const sol::object behavior = state[name];
		if (behavior.get_type() != sol::type::table) {
			return false;
		}

		// A step the behavior leaves out does nothing.
		const sol::object function = behavior.as<sol::table>()[step];
		if (function.get_type() != sol::type::function) {
			return true;
		}

		auto* thread = lua.SpawnThread();
		thread->set_value(agentKey, sol::make_object(state, mAgent.GetId()));

		// Tracked before it runs: a step that ends at once goes back to the pool inside call.
		const auto ticket = keep ? lua.TrackThread(thread) : 0;
		const bool ran = thread->call<void>(function.as<sol::function>());
		if (keep) {
			mTicket = ticket;
		}
		return ran;
	}
}
