
#ifndef _GAME_MODIFIERMANAGER_HEADER
#define _GAME_MODIFIERMANAGER_HEADER

// Include
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

// Game
namespace Game {
	// ActivationType
	// The values of the game's nActivationType table.
	enum class ActivationType : uint8_t {
		Default = 0,
		Unique,
		CasterUnique,
		CasterUniqueIrreplaceable,
		Stacks,
		StacksAndCasterUnique,
		UniqueIrreplaceable,
		UniqueResets
	};

	// DeactivationType
	// The values of the game's nDeactivationType table.
	enum class DeactivationType : uint8_t {
		Default = 0,
		OnAgentDestroyed,
		OnAgentDeath
	};

	// ModifierSpec
	struct ModifierSpec {
		uint32_t guid = 0;
		uint32_t durationMs = 0; // Zero lasts until something removes it
		uint32_t maxStackCount = 0; // Zero has no limit
		ActivationType activation = ActivationType::Default;
		DeactivationType deactivation = DeactivationType::Default;
		bool bound = false; // Blocks actions in the client, not only movement
	};

	// ModifierInstance
	struct ModifierInstance {
		ModifierSpec spec;
		uint64_t startMs = 0;
		uint32_t id = 0;
		uint32_t agent = 0;
		uint32_t initiator = 0;
		uint32_t stackCount = 1;
		bool markedForDelete = false;
	};

	// ModifierDecision
	struct ModifierDecision {
		enum class Action : uint8_t {
			Created,
			Replaced,
			Stacked,
			Refreshed,
			Refused
		};

		Action action = Action::Refused;
		uint32_t id = 0;
		uint32_t replacedId = 0;
	};

	// ModifierDurationRules
	struct ModifierDurationRules {
		float buffIncrease = 0;
		float debuffReduction = 0;
		float dotReduction = 0;
		float crowdControlReduction = 0;

		bool isBuff = false;
		bool isDebuff = false;
		bool isDoT = false;
		bool isCrowdControl = false;
	};

	uint32_t ResolveModifierDuration(float seconds, const ModifierDurationRules& rules);

	// ModifierManager
	// Decides what happens to modifiers: which one a request creates, stacks or replaces, and when
	// each one ends. It knows nothing about scripts or packets.
	class ModifierManager {
		public:
			ModifierDecision Request(const ModifierSpec& spec, uint32_t agent, uint32_t initiator, uint64_t now);

			void Remove(uint32_t id);
			void MarkForDelete(uint32_t id);

			// Ids that are out of time or marked. They stay until they are removed.
			std::vector<uint32_t> GetFinished(uint64_t now) const;

			void ResetDuration(uint32_t id, uint64_t now);
			void SetStackCount(uint32_t id, uint32_t count);

			const ModifierInstance* Get(uint32_t id) const;
			const ModifierInstance* FindFirst(uint32_t agent, uint32_t guid) const;
			std::vector<uint32_t> GetAll(uint32_t agent) const;

			uint32_t GetRemainingMs(uint32_t id, uint64_t now) const;

		private:
			const ModifierInstance* FindExisting(const ModifierSpec& spec, uint32_t agent, uint32_t initiator) const;
			uint32_t Create(const ModifierSpec& spec, uint32_t agent, uint32_t initiator, uint64_t now);

		private:
			std::map<uint32_t, ModifierInstance> mInstances;
			uint32_t mNextId = 1;
	};
}

#endif
