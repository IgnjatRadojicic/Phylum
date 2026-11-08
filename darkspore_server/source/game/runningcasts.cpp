
// Include
#include "runningcasts.h"

#include <algorithm>

// Game
namespace Game {
	void RunningCasts::Add(uint32_t agent, LuaThread* thread) {
		if (agent == 0 || !thread) {
			return;
		}

		Remove(thread);

		const auto serial = mNextSerial++;
		mByThread[thread] = { agent, serial };
		mByAgent[agent].push_back({ thread, serial });
	}

	void RunningCasts::Remove(LuaThread* thread) {
		const auto it = mByThread.find(thread);
		if (it == mByThread.end()) {
			return;
		}

		const auto agent = it->second.agent;
		mByThread.erase(it);

		const auto casts = mByAgent.find(agent);
		if (casts == mByAgent.end()) {
			return;
		}

		std::erase_if(casts->second, [thread](const Entry& entry) { return entry.thread == thread; });
		if (casts->second.empty()) {
			mByAgent.erase(casts);
		}
	}

	std::vector<RunningCasts::Entry> RunningCasts::Of(uint32_t agent) const {
		const auto it = mByAgent.find(agent);
		return it != mByAgent.end() ? it->second : std::vector<Entry> {};
	}

	bool RunningCasts::IsRunning(const Entry& entry) const {
		const auto it = mByThread.find(entry.thread);
		return it != mByThread.end() && it->second.serial == entry.serial;
	}

	bool RunningCasts::Any(uint32_t agent) const {
		return mByAgent.contains(agent);
	}
}
