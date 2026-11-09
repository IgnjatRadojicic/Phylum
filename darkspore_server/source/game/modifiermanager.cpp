
// Include
#include "modifiermanager.h"

#include <algorithm>
#include <cmath>
#include <limits>

// Game
namespace Game {
	namespace {
		bool IsPerCaster(ActivationType type) {
			switch (type) {
				case ActivationType::CasterUnique:
				case ActivationType::CasterUniqueIrreplaceable:
				case ActivationType::StacksAndCasterUnique:
					return true;
				default:
					return false;
			}
		}
	}

	// Functions
	std::optional<uint32_t> ResolveModifierDuration(float seconds, const ModifierDurationRules& rules) {
		const bool timed = seconds > 0;

		if (rules.isBuff) {
			seconds *= 1.f + rules.buffIncrease;
		} else if (rules.isDebuff) {
			seconds *= 1.f - rules.debuffReduction;
		}

		if (rules.isDoT) {
			seconds *= 1.f - rules.dotReduction;
		} else if (rules.isCrowdControl) {
			seconds *= 1.f - rules.crowdControlReduction;
		}

		// Cinematic modifiers use 100000000 seconds to mean forever, which is more than the packet can
		// carry. We do not know if the client reads the field as signed, so this stays positive either way.
		constexpr double longest = std::numeric_limits<int32_t>::max();
		const auto duration = static_cast<uint32_t>(std::min(std::round(std::max(seconds, 0.f) * 1000.0), longest));
		if (timed && duration == 0) {
			return std::nullopt;
		}
		return duration;
	}

	// ModifierManager
	ModifierDecision ModifierManager::Request(const ModifierSpec& spec, uint32_t agent, uint32_t initiator, uint64_t now) {
		using Action = ModifierDecision::Action;

		const auto existing = FindExisting(spec, agent, initiator);
		if (!existing) {
			return { Action::Created, Create(spec, agent, initiator, now) };
		}

		const auto id = existing->id;
		switch (spec.activation) {
			case ActivationType::Unique:
			case ActivationType::CasterUnique:
				return { Action::Replaced, Create(spec, agent, initiator, now), id };

			case ActivationType::Stacks:
			case ActivationType::StacksAndCasterUnique:
				return { Action::Stacked, id };

			case ActivationType::UniqueResets:
				ResetDuration(id, now);
				return { Action::Refreshed, id };

			default:
				return { Action::Refused, id };
		}
	}

	void ModifierManager::Remove(uint32_t id) {
		mInstances.erase(id);
	}

	void ModifierManager::MarkForDelete(uint32_t id) {
		if (auto it = mInstances.find(id); it != mInstances.end()) {
			it->second.markedForDelete = true;
		}
	}

	std::vector<uint32_t> ModifierManager::GetFinished(uint64_t now) const {
		std::vector<uint32_t> finished;
		for (const auto& [id, instance] : mInstances) {
			const auto duration = instance.spec.durationMs;
			if (instance.markedForDelete || (duration > 0 && now >= instance.startMs + duration)) {
				finished.push_back(id);
			}
		}
		return finished;
	}

	void ModifierManager::ResetDuration(uint32_t id, uint64_t now) {
		if (auto it = mInstances.find(id); it != mInstances.end()) {
			it->second.startMs = now;
		}
	}

	void ModifierManager::SetStackCount(uint32_t id, uint32_t count) {
		if (auto it = mInstances.find(id); it != mInstances.end()) {
			const auto limit = it->second.spec.maxStackCount;
			it->second.stackCount = limit > 0 ? std::min(count, limit) : count;
		}
	}

	const ModifierInstance* ModifierManager::Get(uint32_t id) const {
		const auto it = mInstances.find(id);
		return it != mInstances.end() ? &it->second : nullptr;
	}

	const ModifierInstance* ModifierManager::FindFirst(uint32_t agent, uint32_t guid) const {
		for (const auto& [_, instance] : mInstances) {
			if (instance.agent == agent && instance.spec.guid == guid && !instance.markedForDelete) {
				return &instance;
			}
		}
		return nullptr;
	}

	std::vector<uint32_t> ModifierManager::GetAll(uint32_t agent) const {
		std::vector<uint32_t> ids;
		for (const auto& [id, instance] : mInstances) {
			if (instance.agent == agent) {
				ids.push_back(id);
			}
		}
		return ids;
	}

	bool ModifierManager::HasAny(uint32_t agent) const {
		for (const auto& [_, instance] : mInstances) {
			if (instance.agent == agent) {
				return true;
			}
		}
		return false;
	}

	bool ModifierManager::HasAny(uint32_t agent, uint32_t guid) const {
		for (const auto& [_, instance] : mInstances) {
			if (instance.agent == agent && instance.spec.guid == guid) {
				return true;
			}
		}
		return false;
	}

	uint32_t ModifierManager::GetRemainingMs(uint32_t id, uint64_t now) const {
		const auto instance = Get(id);
		if (!instance || instance->spec.durationMs == 0) {
			return 0;
		}

		const auto end = instance->startMs + instance->spec.durationMs;
		return now < end ? static_cast<uint32_t>(end - now) : 0;
	}

	const ModifierInstance* ModifierManager::FindExisting(const ModifierSpec& spec, uint32_t agent, uint32_t initiator) const {
		if (spec.activation == ActivationType::Default) {
			return nullptr;
		}

		const bool perCaster = IsPerCaster(spec.activation);
		for (const auto& [_, instance] : mInstances) {
			if (instance.agent == agent && instance.spec.guid == spec.guid && !instance.markedForDelete &&
				(!perCaster || instance.initiator == initiator)) {
				return &instance;
			}
		}
		return nullptr;
	}

	uint32_t ModifierManager::Create(const ModifierSpec& spec, uint32_t agent, uint32_t initiator, uint64_t now) {
		const auto id = mNextId++;

		auto& instance = mInstances[id];
		instance.spec = spec;
		instance.startMs = now;
		instance.id = id;
		instance.agent = agent;
		instance.initiator = initiator;
		return id;
	}
}
