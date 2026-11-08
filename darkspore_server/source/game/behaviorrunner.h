
#ifndef _GAME_BEHAVIORRUNNER_HEADER
#define _GAME_BEHAVIORRUNNER_HEADER

// Include
#include <cstdint>
#include <string>

// Game
namespace Game {
	class Object;

	// BehaviorRunner
	// Runs one AI behavior for an agent as the client does. A behavior is a global table
	// (nBehavior_FollowOwner): [1] runs once when it starts (0xa68f10), [2] runs as a coroutine
	// that the server resumes every update for as long as it waits (0xa68eb0), and [3] runs when it
	// is left (0xa68e10), where every pet behavior stops its agent. Each step runs on its own
	// thread, which nBehaviorTree.GetMyObjectID reads its agent from.
	class BehaviorRunner {
		public:
			// The thread value GetMyObjectID reads.
			static constexpr const char* agentKey = "behaviorAgent";

			explicit BehaviorRunner(Object& agent);

			BehaviorRunner(const BehaviorRunner&) = delete;
			BehaviorRunner& operator=(const BehaviorRunner&) = delete;

			// Starts the named behavior unless it runs already; one that ended starts again. A name
			// no script defines leaves the current behavior and runs nothing.
			void Run(const std::string& name);

			// Leaves the running behavior, if any: its coroutine stops and its exit step runs.
			// Call it before the agent goes (ObjectManager), while its scripts can still reach it.
			void Exit();

		private:
			bool Call(const std::string& name, int step, bool keep);

		private:
			Object& mAgent;
			std::string mName;
			uint64_t mTicket = 0;
	};
}

#endif
