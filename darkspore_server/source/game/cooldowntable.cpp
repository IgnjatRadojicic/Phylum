
// Include
#include "cooldowntable.h"

#include <algorithm>

// Game
namespace Game {
	bool CooldownTable::IsCooling(uint32_t abilityId, uint64_t now) const {
		const auto it = mSpans.find(abilityId);
		return it != mSpans.end() && it->second.end > now;
	}

	std::optional<CooldownSpan> CooldownTable::Get(uint32_t abilityId) const {
		const auto it = mSpans.find(abilityId);
		return it != mSpans.end() ? std::optional(it->second) : std::nullopt;
	}

	CooldownSpan CooldownTable::Start(uint32_t abilityId, uint64_t now, uint32_t milliseconds) {
		return mSpans[abilityId] = CooldownSpan { now, now + milliseconds };
	}

	std::optional<CooldownSpan> CooldownTable::Shift(uint32_t abilityId, int64_t milliseconds) {
		const auto it = mSpans.find(abilityId);
		if (it == mSpans.end()) {
			return std::nullopt;
		}
		auto& span = it->second;
		const int64_t end = static_cast<int64_t>(span.end) + milliseconds;
		span.end = static_cast<uint64_t>(std::max<int64_t>(0, end));
		span.start = std::min(span.start, span.end);
		return span;
	}

	std::optional<CooldownSpan> CooldownTable::Clear(uint32_t abilityId, uint64_t now) {
		const auto it = mSpans.find(abilityId);
		if (it == mSpans.end()) {
			return std::nullopt;
		}
		auto& span = it->second;
		span.end = now;
		span.start = std::min(span.start, now);
		return span;
	}

	CooldownTable::Changes CooldownTable::ClearAll(uint64_t now) {
		Changes changes;
		for (const auto& [abilityId, _] : mSpans) {
			changes.emplace_back(abilityId, *Clear(abilityId, now));
		}
		return changes;
	}

	std::optional<CooldownSpan> CooldownTable::Scale(uint32_t abilityId, uint64_t now, float scale) {
		const auto it = mSpans.find(abilityId);
		if (it == mSpans.end() || it->second.end <= now) {
			return std::nullopt;
		}
		auto& span = it->second;
		const auto left = static_cast<float>(span.end - now);
		const auto change = static_cast<int64_t>((scale - 1.f) * left);
		span.end = static_cast<uint64_t>(std::max<int64_t>(static_cast<int64_t>(now), static_cast<int64_t>(span.end) + change));
		return span;
	}

	CooldownTable::Changes CooldownTable::ScaleAll(uint64_t now, float scale) {
		Changes changes;
		for (const auto& [abilityId, _] : mSpans) {
			if (const auto span = Scale(abilityId, now, scale)) {
				changes.emplace_back(abilityId, *span);
			}
		}
		return changes;
	}
}
