
#include <doctest/doctest.h>

#include "game/attributes.h"
#include "game/attributestack.h"

using Game::AttributeFromScript;
using Game::AttributeOp;
using Game::AttributeStack;
using Game::AttributeType;

namespace {
	constexpr uint8_t attribute = 5;

	uint8_t Id(AttributeType type) {
		return static_cast<uint8_t>(type);
	}
}

TEST_CASE("adds sum on top of the base") {
	AttributeStack stack;
	stack.Add(attribute, 2, AttributeOp::Add, 10);
	stack.Add(attribute, -5, AttributeOp::Add, 12);

	CHECK(stack.Resolve(attribute) == doctest::Approx(7));
}

TEST_CASE("percents apply to the base and adds together") {
	AttributeStack stack;
	stack.Add(attribute, 0.5f, AttributeOp::PercentChange, 10);
	stack.Add(attribute, 10, AttributeOp::Add, 15);

	CHECK(stack.Resolve(attribute) == doctest::Approx(30));
}

TEST_CASE("only the best of the best changes counts") {
	AttributeStack stack;
	stack.Add(attribute, 3, AttributeOp::AddBest, 10);
	stack.Add(attribute, 7, AttributeOp::AddBest, 10);
	stack.Add(attribute, 0.1f, AttributeOp::PercentChangeBest, 10);
	stack.Add(attribute, 0.5f, AttributeOp::PercentChangeBest, 10);

	CHECK(stack.Resolve(attribute) == doctest::Approx(25.5f));
}

TEST_CASE("the lowest clamp limits the result") {
	AttributeStack stack;
	stack.Add(attribute, 8, AttributeOp::Clamp, 10);
	stack.Add(attribute, 4, AttributeOp::Clamp, 10);

	CHECK(stack.Resolve(attribute) == doctest::Approx(4));
}

TEST_CASE("removing every change gives the base back") {
	AttributeStack stack;
	const auto first = stack.Add(attribute, 2, AttributeOp::Add, 10);
	const auto second = stack.Add(attribute, 0.5f, AttributeOp::PercentChange, 10);

	CHECK(stack.Remove(first) == attribute);
	CHECK(stack.Remove(second) == attribute);
	CHECK_FALSE(stack.Has(attribute));
	CHECK(stack.Resolve(attribute) == doctest::Approx(10));
}

TEST_CASE("a change after the old ones ended starts from the current value") {
	AttributeStack stack;
	const auto first = stack.Add(attribute, 2, AttributeOp::Add, 10);
	stack.Remove(first);

	stack.Add(attribute, 1, AttributeOp::Add, 30);

	CHECK(stack.GetBase(attribute) == doctest::Approx(30));
	CHECK(stack.Resolve(attribute) == doctest::Approx(31));
}

TEST_CASE("an unknown handle changes nothing") {
	AttributeStack stack;
	stack.Add(attribute, 2, AttributeOp::Add, 10);

	CHECK_FALSE(stack.Remove(999));
	CHECK(stack.Resolve(attribute) == doctest::Approx(12));
}

TEST_CASE("a new base keeps the changes") {
	AttributeStack stack;
	stack.Add(attribute, 2, AttributeOp::Add, 10);
	stack.SetBase(attribute, 20);

	CHECK(stack.Resolve(attribute) == doctest::Approx(22));
}

TEST_CASE("script attribute numbers become server attributes") {
	CHECK(AttributeFromScript(12) == Id(AttributeType::CombatSpeed));
	CHECK(AttributeFromScript(48) == Id(AttributeType::MovementSpeedBuff));
	CHECK(AttributeFromScript(73) == Id(AttributeType::ImmuneToStunned));
	CHECK(AttributeFromScript(74) == Id(AttributeType::ImmuneToShocked));
	CHECK(AttributeFromScript(75) == Id(AttributeType::ImmuneToSleep));
	CHECK(AttributeFromScript(76) == Id(AttributeType::ImmuneToTaunted));
	CHECK(AttributeFromScript(83) == Id(AttributeType::ImmuneToSlow));
	CHECK(AttributeFromScript(99) == Id(AttributeType::ProjectileDamageIncrease));
	CHECK(AttributeFromScript(100) == Id(AttributeType::DistributeDamageAmongSquad));
	CHECK(AttributeFromScript(101) == Id(AttributeType::DeployBonusInvincibilityTime));
	CHECK(AttributeFromScript(113) == Id(AttributeType::BodyScale));
	CHECK(AttributeFromScript(114) == Id(AttributeType::DefenseBoostPhysicalDamage));
}
