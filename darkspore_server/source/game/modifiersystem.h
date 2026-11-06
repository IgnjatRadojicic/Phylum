
#ifndef _GAME_MODIFIERSYSTEM_HEADER
#define _GAME_MODIFIERSYSTEM_HEADER

// Include
#include "attributestack.h"
#include "modifiermanager.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

// Game
namespace Game {
	class Instance;

	// ModifierSystem
	// Runs modifiers in a game: ModifierManager's decisions become script calls, attribute changes
	// and packets. Scripts reach it through the adapter in scriptapi.cpp.
	class ModifierSystem {
		public:
			explicit ModifierSystem(Instance& game);

			// The context is the table a modifier's script functions run with. Returns the instance
			// id, or zero if the request was refused.
			uint32_t Request(const ModifierSpec& spec, const ModifierDurationRules& rules, float seconds, uint32_t agent, uint32_t initiator, sol::table context);

			// Ends what has run out. Scripts only mark modifiers, because one may be ending itself.
			void Update();

			void OnAgentDeath(uint32_t agent);
			void OnAgentDestroyed(uint32_t agent);

			void MarkForDelete(uint32_t id);
			void ResetDuration(uint32_t id);
			void SetStackCount(uint32_t id, uint32_t count);

			uint32_t AddAttributeChange(uint32_t object, uint8_t attribute, float value, AttributeOp op, uint32_t id);
			void RemoveAttributeChange(uint32_t object, uint32_t handle, uint32_t id);

			const ModifierManager& GetManager() const;
			sol::object GetContext(uint32_t id, sol::this_state L) const;

		private:
			struct AttributeChange {
				uint32_t object = 0;
				uint32_t handle = 0;
			};

			struct Run {
				// Held on the main state: the thread that made the table is reused or freed long before this.
				sol::main_table context;
				std::vector<AttributeChange> changes;
				uint64_t ticket = 0;
			};

		private:
			void Start(uint32_t id, sol::table context);
			void Stack(uint32_t id);
			void Finish(uint32_t id);

			bool CallScript(const char* function, sol::table context);
			void SendUpdated(uint32_t id, bool restarted);

		private:
			Instance& mGame;

			ModifierManager mManager;
			std::unordered_map<uint32_t, Run> mRuns;
	};
}

#endif
