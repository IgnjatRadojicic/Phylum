
#ifndef _GAME_COOLDOWNTABLE_HEADER
#define _GAME_COOLDOWNTABLE_HEADER

// Include
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

// Game
namespace Game {
	// CooldownSpan
	// When an ability's cooldown started and when it ends, in game milliseconds.
	struct CooldownSpan {
		uint64_t start = 0;
		uint64_t end = 0;

		uint32_t Duration() const { return end > start ? static_cast<uint32_t>(end - start) : 0; }
	};

	// CooldownTable
	// One object's ability cooldowns, changed the way the client's own nAbility functions do
	// (Darkspore.exe): an ability keeps its entry once it has cooled down, and each change
	// moves its end.
	class CooldownTable {
		public:
			using Changes = std::vector<std::pair<uint32_t, CooldownSpan>>;

			bool IsCooling(uint32_t abilityId, uint64_t now) const;
			std::optional<CooldownSpan> Get(uint32_t abilityId) const;

			// ResetAbilityCooldown (0x9dfc70) and paying for a cast: cools down from now.
			CooldownSpan Start(uint32_t abilityId, uint64_t now, uint32_t milliseconds);

			// AddCooldownTime (0x9dfe80): moves the end of an ability that has an entry.
			std::optional<CooldownSpan> Shift(uint32_t abilityId, int64_t milliseconds);

			// RemoveCooldownTime (0x9dff10): ends one cooldown now, or every one.
			std::optional<CooldownSpan> Clear(uint32_t abilityId, uint64_t now);
			Changes ClearAll(uint64_t now);

			// ScaleCooldownTime (0x9dffb0): keeps scale of what is left of a running cooldown.
			std::optional<CooldownSpan> Scale(uint32_t abilityId, uint64_t now, float scale);
			Changes ScaleAll(uint64_t now, float scale);

		private:
			std::map<uint32_t, CooldownSpan> mSpans;
	};
}

#endif
