
#include <doctest/doctest.h>

#include "game/modifiermanager.h"

using namespace Game;
using Action = ModifierDecision::Action;

namespace {
	constexpr uint32_t agent = 10;
	constexpr uint32_t casterA = 20;
	constexpr uint32_t casterB = 21;
	constexpr uint64_t start = 1000;

	ModifierSpec Spec(ActivationType activation, uint32_t durationMs = 5000) {
		ModifierSpec spec;
		spec.guid = 0xABCD;
		spec.activation = activation;
		spec.durationMs = durationMs;
		return spec;
	}
}

TEST_CASE("a first request always creates a modifier") {
	for (auto type : { ActivationType::Default, ActivationType::Unique, ActivationType::Stacks, ActivationType::UniqueIrreplaceable }) {
		ModifierManager manager;
		const auto decision = manager.Request(Spec(type), agent, casterA, start);

		CHECK(decision.action == Action::Created);
		CHECK(manager.Get(decision.id)->stackCount == 1);
	}
}

TEST_CASE("default modifiers run side by side") {
	ModifierManager manager;
	const auto first = manager.Request(Spec(ActivationType::Default), agent, casterA, start);
	const auto second = manager.Request(Spec(ActivationType::Default), agent, casterA, start);

	CHECK(second.action == Action::Created);
	CHECK(first.id != second.id);
	CHECK(manager.GetAll(agent).size() == 2);
}

TEST_CASE("a unique modifier replaces the one from any caster") {
	ModifierManager manager;
	const auto first = manager.Request(Spec(ActivationType::Unique), agent, casterA, start);
	const auto second = manager.Request(Spec(ActivationType::Unique), agent, casterB, start);

	CHECK(second.action == Action::Replaced);
	CHECK(second.replacedId == first.id);
}

TEST_CASE("a caster unique modifier only replaces its own caster's") {
	ModifierManager manager;
	const auto first = manager.Request(Spec(ActivationType::CasterUnique), agent, casterA, start);

	CHECK(manager.Request(Spec(ActivationType::CasterUnique), agent, casterB, start).action == Action::Created);

	const auto again = manager.Request(Spec(ActivationType::CasterUnique), agent, casterA, start);
	CHECK(again.action == Action::Replaced);
	CHECK(again.replacedId == first.id);
}

TEST_CASE("irreplaceable modifiers refuse a second request") {
	ModifierManager manager;
	manager.Request(Spec(ActivationType::UniqueIrreplaceable), agent, casterA, start);
	CHECK(manager.Request(Spec(ActivationType::UniqueIrreplaceable), agent, casterB, start).action == Action::Refused);

	ModifierManager perCaster;
	perCaster.Request(Spec(ActivationType::CasterUniqueIrreplaceable), agent, casterA, start);
	CHECK(perCaster.Request(Spec(ActivationType::CasterUniqueIrreplaceable), agent, casterA, start).action == Action::Refused);
	CHECK(perCaster.Request(Spec(ActivationType::CasterUniqueIrreplaceable), agent, casterB, start).action == Action::Created);
}

TEST_CASE("stacking modifiers report a stack and leave the count to the caller") {
	ModifierManager manager;
	const auto first = manager.Request(Spec(ActivationType::Stacks), agent, casterA, start);
	const auto second = manager.Request(Spec(ActivationType::Stacks), agent, casterB, start);

	CHECK(second.action == Action::Stacked);
	CHECK(second.id == first.id);
	CHECK(manager.Get(first.id)->stackCount == 1);
}

TEST_CASE("stacks and caster unique modifiers stack per caster") {
	ModifierManager manager;
	manager.Request(Spec(ActivationType::StacksAndCasterUnique), agent, casterA, start);

	CHECK(manager.Request(Spec(ActivationType::StacksAndCasterUnique), agent, casterB, start).action == Action::Created);
	CHECK(manager.Request(Spec(ActivationType::StacksAndCasterUnique), agent, casterA, start).action == Action::Stacked);
}

TEST_CASE("a resetting modifier starts its time again") {
	ModifierManager manager;
	const auto first = manager.Request(Spec(ActivationType::UniqueResets), agent, casterA, start);
	const auto second = manager.Request(Spec(ActivationType::UniqueResets), agent, casterA, start + 4000);

	CHECK(second.action == Action::Refreshed);
	CHECK(second.id == first.id);
	CHECK(manager.GetRemainingMs(first.id, start + 4000) == 5000);
}

TEST_CASE("the stack count stops at the limit") {
	auto spec = Spec(ActivationType::Stacks);
	spec.maxStackCount = 3;

	ModifierManager manager;
	const auto id = manager.Request(spec, agent, casterA, start).id;

	manager.SetStackCount(id, 9);
	CHECK(manager.Get(id)->stackCount == 3);
}

TEST_CASE("a modifier finishes when its time is up") {
	ModifierManager manager;
	const auto id = manager.Request(Spec(ActivationType::Default), agent, casterA, start).id;

	CHECK(manager.GetFinished(start + 4999).empty());
	CHECK(manager.GetFinished(start + 5000) == std::vector<uint32_t> { id });
}

TEST_CASE("a modifier with no duration lasts until it is marked") {
	ModifierManager manager;
	const auto id = manager.Request(Spec(ActivationType::Default, 0), agent, casterA, start).id;

	CHECK(manager.GetFinished(start + 1000000).empty());

	manager.MarkForDelete(id);
	CHECK(manager.GetFinished(start) == std::vector<uint32_t> { id });
}

TEST_CASE("a marked modifier no longer blocks a new one") {
	ModifierManager manager;
	const auto first = manager.Request(Spec(ActivationType::UniqueIrreplaceable), agent, casterA, start);

	manager.MarkForDelete(first.id);

	CHECK(manager.Request(Spec(ActivationType::UniqueIrreplaceable), agent, casterA, start).action == Action::Created);
	CHECK(manager.FindFirst(agent, 0xABCD)->id != first.id);
}

TEST_CASE("durations follow buffs, debuffs and the target's resistances") {
	ModifierDurationRules buff;
	buff.isBuff = true;
	buff.buffIncrease = 0.5f;
	CHECK(ResolveModifierDuration(2, buff) == 3000u);

	ModifierDurationRules stun;
	stun.isDebuff = true;
	stun.isCrowdControl = true;
	stun.debuffReduction = 0.5f;
	stun.crowdControlReduction = 0.5f;
	CHECK(ResolveModifierDuration(4, stun) == 1000u);

	ModifierDurationRules dot;
	dot.isDoT = true;
	dot.dotReduction = 2;
	CHECK_FALSE(ResolveModifierDuration(4, dot).has_value());
}

TEST_CASE("a duration too long for the client stops at the longest it can hold") {
	const ModifierDurationRules rules;

	CHECK(ResolveModifierDuration(100000000, rules) == 0x7FFFFFFFu);
}

TEST_CASE("resistance cannot make a timed modifier last forever") {
	ModifierDurationRules stun;
	stun.isCrowdControl = true;
	stun.crowdControlReduction = 1;

	CHECK_FALSE(ResolveModifierDuration(4, stun).has_value());
	CHECK(ResolveModifierDuration(0, stun) == 0u);
}

TEST_CASE("an agent holds a GUID until its last modifier of it is removed, marked or not") {
	ModifierManager manager;
	const auto first = manager.Request(Spec(ActivationType::Default), agent, casterA, start).id;
	const auto second = manager.Request(Spec(ActivationType::Default), agent, casterB, start).id;
	CHECK(manager.HasAny(agent));
	CHECK(manager.HasAny(agent, 0xABCD));
	CHECK_FALSE(manager.HasAny(agent, 0x1234));

	// Two ending in one frame: the first one's end must still see the marked second.
	manager.MarkForDelete(second);
	manager.Remove(first);
	CHECK(manager.HasAny(agent, 0xABCD));
	CHECK(manager.FindFirst(agent, 0xABCD) == nullptr);

	manager.Remove(second);
	CHECK_FALSE(manager.HasAny(agent, 0xABCD));
	CHECK_FALSE(manager.HasAny(agent));
}
