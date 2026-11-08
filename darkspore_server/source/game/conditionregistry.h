
#ifndef _GAME_CONDITIONREGISTRY_HEADER
#define _GAME_CONDITIONREGISTRY_HEADER

// Include
#include "aiproperty.h"
#include "lua.h"

#include <glm/glm.hpp>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Game
namespace Game {
	// ConditionResult
	// What a condition said about its agent: whether it holds, and the target and point it picked.
	struct ConditionResult {
		bool passed = false;
		uint32_t targetId = 0;
		std::optional<glm::vec3> position;
	};

	// ConditionRegistry
	// The engine side of nCondition. A condition script registers a table under a name
	// (RegisterCondition, Darkspore.exe 0xa67e10) and sets its function [1] after that. The function
	// takes the agent's id and gives back whether the condition holds, then x, y, z and a target id
	// (0xa330c0). A condition nobody registered holds (0xa33170), so a gambit without one always goes.
	// While a condition runs, GetProperty gives the gambit's own value for a name, else the default
	// in the condition's properties table, else nothing (0xa676e0, 0xa67430).
	class ConditionRegistry {
		public:
			using TraceFn = std::function<void(const std::string&)>;

			explicit ConditionRegistry(TraceFn trace);

			void Register(const std::string& name, sol::table condition);
			size_t Size() const;

			ConditionResult Evaluate(uint32_t conditionId, const AIProperties& properties, uint32_t agentId);

			const AIProperty* GetProperty(const std::string& name) const;

		private:
			struct Entry {
				std::string name;
				sol::table table;
				AIProperties defaults;
				bool defaultsRead = false;
			};

			struct Scope {
				const AIProperties* properties = nullptr;
				const Entry* entry = nullptr;
			};

			static AIProperties ReadDefaults(const sol::table& table);
			void ReportOnce(const Entry& entry, const std::string& message);

		private:
			std::unordered_map<uint32_t, Entry> mConditions;
			std::unordered_set<uint32_t> mReported;
			std::vector<Scope> mScopes;
			TraceFn mTrace;
	};
}

#endif
