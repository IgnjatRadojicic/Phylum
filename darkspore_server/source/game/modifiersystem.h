
#ifndef _GAME_MODIFIERSYSTEM_HEADER
#define _GAME_MODIFIERSYSTEM_HEADER

// Include
#include "abilityevent.h"
#include "attributestack.h"
#include "modifiermanager.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Game
namespace Game {
	class Instance;
	class Object;

	// ModifierSystem
	// Runs modifiers in a game: ModifierManager's decisions become script calls, attribute changes
	// and packets. Scripts reach it through the adapter in scriptapi.cpp.
	class ModifierSystem {
		public:
			explicit ModifierSystem(Instance& game);

			// Hands an event (made by AbilityEventDelivery) to each modifier running on the agent. One
			// whose handler answers false ends.
			void DeliverEvent(uint32_t agent, const sol::table& data);
			bool HasModifiers(uint32_t agent) const;

			// The context is the table a modifier's script functions run with. Returns the instance
			// id, or zero if the request was refused.
			uint32_t Request(const ModifierSpec& spec, const ModifierDurationRules& rules, float seconds, uint32_t agent, uint32_t initiator, sol::table context);

			// Ends what has run out. Scripts only mark modifiers, because one may be ending itself.
			void Update();

			void OnAgentDeath(uint32_t agent);
			void OnAgentDestroyed(uint32_t agent);

			// Ends every modifier on the agent now, running each Deactivate before this returns.
			void EndAll(uint32_t agent);

			void MarkForDelete(uint32_t id);
			bool IsRunning(uint32_t id) const;

			// A thread the modifier's script started, which is stopped when the modifier ends.
			// Returns false when the modifier is not running.
			bool AdoptThread(uint32_t id, uint64_t ticket);
			void ResetDuration(uint32_t id);
			void SetStackCount(uint32_t id, uint32_t count);

			uint32_t AddAttributeChange(uint32_t object, uint8_t attribute, float value, AttributeOp op, uint32_t id);
			void RemoveAttributeChange(uint32_t object, uint32_t handle, uint32_t id);

			const ModifierManager& GetManager() const;
			sol::object GetContext(uint32_t id, sol::this_state L) const;

		private:
			// Held by reference, because object ids are used again once an object has gone.
			struct AttributeChange {
				std::weak_ptr<Object> object;
				uint32_t handle = 0;
			};

			struct Run {
				// Held on the main state: the thread that made the table is reused or freed long before this.
				sol::main_table context;
				std::vector<AttributeChange> changes;
				uint64_t ticket = 0;
				std::vector<uint64_t> children;
			};

		private:
			void Start(uint32_t id, sol::table context);
			void Stack(uint32_t id);
			void Finish(uint32_t id);
			void EndByEvent(uint32_t id);
			static bool Listens(const sol::table& context, uint32_t type);

			bool CallScript(const char* function, sol::table context, sol::object argument = sol::lua_nil);
			void SendUpdated(uint32_t id, bool restarted);
			void Trace(const char* what, uint32_t guid, uint32_t id, uint32_t agent, uint32_t initiator);

		private:
			Instance& mGame;

			ModifierManager mManager;
			std::unordered_map<uint32_t, Run> mRuns;
			// Ids inside Finish, so a deactivate script cannot end its own modifier a second time.
			std::unordered_set<uint32_t> mFinishing;
	};
}

#endif
