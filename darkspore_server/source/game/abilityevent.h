
#ifndef _GAME_ABILITYEVENT_HEADER
#define _GAME_ABILITYEVENT_HEADER

// Include
#include <array>
#include <cstddef>
#include <cstdint>

// Game
namespace Game {
	// The events scripts subscribe to with handledEvents (nAbilityEventFlags in the game's
	// GlobalDefinitions.lua). Only the ones the server sends are listed.
	enum class AbilityEventType : uint32_t {
		TookDamage = 1,
		UsedAbility = 4,
		StackModifier = 32,
		NearbyDeath = 64,
		DealtDamage = 8192,
		Dodged = 65536
	};

	// AbilityEvent
	// What a script's handleEvent reads with nAbility.GetAbilityEventGUIDData, FloatData and
	// IntData. Each kind of data has its own numbered slots, counted from 1 as the scripts do.
	// Slots nobody set read as zero.
	class AbilityEvent {
		public:
			static constexpr size_t SlotCount = 8;

			explicit AbilityEvent(AbilityEventType type);

			AbilityEventType GetType() const;

			uint64_t GetGUID(size_t slot) const;
			float GetFloat(size_t slot) const;
			int64_t GetInt(size_t slot) const;

			// A slot outside 1..SlotCount is ignored.
			AbilityEvent& SetGUID(size_t slot, uint64_t value);
			AbilityEvent& SetFloat(size_t slot, float value);
			AbilityEvent& SetInt(size_t slot, int64_t value);

		private:
			AbilityEventType mType;

			std::array<uint64_t, SlotCount> mGUIDs {};
			std::array<float, SlotCount> mFloats {};
			std::array<int64_t, SlotCount> mInts {};
	};

	// AbilityEventSink
	// Where events for an object's running scripts go. ModifierSystem is the real one.
	class AbilityEventSink {
		public:
			virtual ~AbilityEventSink() = default;

			virtual void SendEvent(uint32_t agent, const AbilityEvent& event) = 0;
	};
}

#endif
