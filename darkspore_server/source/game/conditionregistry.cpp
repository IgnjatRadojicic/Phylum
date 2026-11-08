
// Include
#include "conditionregistry.h"

#include "utils/functions.h"

#include <format>

// Game
namespace Game {
	namespace {
		bool IsTrue(const sol::object& value) {
			switch (value.get_type()) {
				case sol::type::none:
				case sol::type::lua_nil:
					return false;
				case sol::type::boolean:
					return value.as<bool>();
				default:
					return true;
			}
		}

		std::optional<double> Number(const sol::protected_function_result& result, int index) {
			if (index >= result.return_count()) {
				return std::nullopt;
			}
			const sol::object value = result[index];
			if (value.get_type() != sol::type::number) {
				return std::nullopt;
			}
			return value.as<double>();
		}

		// Scripts write default values as text ("30.0"), but a number or bool reads the same way.
		std::string Text(const sol::object& value) {
			switch (value.get_type()) {
				case sol::type::string:
					return value.as<std::string>();
				case sol::type::number:
					return std::format("{}", value.as<double>());
				case sol::type::boolean:
					return value.as<bool>() ? "true" : "false";
				default:
					return {};
			}
		}
	}

	ConditionRegistry::ConditionRegistry(TraceFn trace) : mTrace(std::move(trace)) {}

	void ConditionRegistry::Register(const std::string& name, sol::table condition) {
		auto& entry = mConditions[utils::hash_id(name)];
		entry.name = name;
		entry.table = std::move(condition);
		entry.defaultsRead = false;
	}

	bool ConditionRegistry::Contains(uint32_t conditionId) const {
		return mConditions.contains(conditionId);
	}

	size_t ConditionRegistry::Size() const {
		return mConditions.size();
	}

	ConditionResult ConditionRegistry::Evaluate(uint32_t conditionId, const AIProperties& properties, uint32_t agentId) {
		const auto it = mConditions.find(conditionId);
		if (it == mConditions.end()) {
			return { .passed = true };
		}

		auto& entry = it->second;
		// Scripts fill the properties in before they register, and the function after.
		if (!entry.defaultsRead) {
			entry.defaults = ReadDefaults(entry.table);
			entry.defaultsRead = true;
		}

		const sol::object function = entry.table[1];
		if (function.get_type() != sol::type::function) {
			ReportOnce(entry, "has no function");
			return {};
		}

		mScopes.push_back({ &properties, &entry });
		const sol::protected_function_result result = function.as<sol::protected_function>()(agentId);
		mScopes.pop_back();

		if (!result.valid()) {
			const sol::error error = result;
			ReportOnce(entry, error.what());
			return {};
		}

		ConditionResult answer;
		answer.passed = result.return_count() > 0 && IsTrue(result[0]);
		if (!answer.passed) {
			return answer;
		}

		const auto x = Number(result, 1), y = Number(result, 2), z = Number(result, 3);
		if (x && y && z && (*x != 0 || *y != 0 || *z != 0)) {
			answer.position = glm::vec3(static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*z));
		}
		if (const auto target = Number(result, 4)) {
			answer.targetId = static_cast<uint32_t>(*target);
		}
		return answer;
	}

	const AIProperty* ConditionRegistry::GetProperty(const std::string& name) const {
		if (mScopes.empty()) {
			return nullptr;
		}

		const auto& scope = mScopes.back();
		const auto id = utils::hash_id(name);
		if (const auto* property = FindAIProperty(*scope.properties, id)) {
			return property;
		}
		return FindAIProperty(scope.entry->defaults, id);
	}

	AIProperties ConditionRegistry::ReadDefaults(const sol::table& table) {
		AIProperties defaults;
		const sol::object list = table["properties"];
		if (list.get_type() != sol::type::table) {
			return defaults;
		}

		for (const auto& [_, value] : list.as<sol::table>()) {
			if (value.get_type() != sol::type::table) {
				continue;
			}
			const auto property = value.as<sol::table>();
			// The type is kReflectionType_*, nUtil.ToGUID of the type's hash.
			const sol::object type = property["type"];
			defaults.push_back(AIProperty::Make(
				Text(property["name"]),
				type.get_type() == sol::type::number ? static_cast<uint32_t>(type.as<double>()) : 0u,
				Text(property["value"])));
		}
		return defaults;
	}

	void ConditionRegistry::ReportOnce(const Entry& entry, const std::string& message) {
		if (mReported.insert(utils::hash_id(entry.name)).second && mTrace) {
			mTrace("ERROR condition " + entry.name + " " + message);
		}
	}
}
