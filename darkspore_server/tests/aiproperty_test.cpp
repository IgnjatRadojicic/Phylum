
#include <doctest/doctest.h>

#include "game/aiproperty.h"

#include "utils/functions.h"

using Game::AIProperty;
using Game::FindAIProperty;
namespace Type = Game::AIPropertyType;

TEST_CASE("a float property reads as a number") {
	const auto radius = AIProperty::Make("Radius", Type::Float, "20");
	REQUIRE(std::holds_alternative<double>(radius.value));
	CHECK(std::get<double>(radius.value) == doctest::Approx(20.0));
	CHECK(radius.nameId == utils::hash_id("radius"));

	const auto distance = AIProperty::Make("Distance", Type::Float, "10.0");
	CHECK(std::get<double>(distance.value) == doctest::Approx(10.0));
}

TEST_CASE("a bool property is true only for true") {
	CHECK(std::get<bool>(AIProperty::Make("GreaterThan", Type::Bool, "true").value));
	CHECK_FALSE(std::get<bool>(AIProperty::Make("GreaterThan", Type::Bool, "false").value));
	CHECK_FALSE(std::get<bool>(AIProperty::Make("GreaterThan", Type::Bool, "1").value));
}

TEST_CASE("an int property reads its leading digits and sign") {
	CHECK(std::get<double>(AIProperty::Make("Count", Type::Int, "12abc").value) == doctest::Approx(12.0));
	CHECK(std::get<double>(AIProperty::Make("Count", Type::Int, "-3").value) == doctest::Approx(-3.0));
}

TEST_CASE("an unsigned property reads hex after 0x") {
	CHECK(std::get<double>(AIProperty::Make("Id", Type::UInt, "0x10").value) == doctest::Approx(16.0));
	CHECK(std::get<double>(AIProperty::Make("Id", Type::Guid, "42").value) == doctest::Approx(42.0));
}

TEST_CASE("a string property keeps its text, and an unknown type gives nothing") {
	CHECK(std::get<std::string>(AIProperty::Make("Name", Type::String, "Fireball").value) == "Fireball");
	CHECK(std::holds_alternative<std::monostate>(AIProperty::Make("Odd", 1234, "5").value));
}

TEST_CASE("a property is found by its name, whatever the case") {
	const Game::AIProperties properties { AIProperty::Make("Distance", Type::Float, "8"), AIProperty::Make("GreaterThan", Type::Bool, "true") };
	const auto found = FindAIProperty(properties, utils::hash_id("greaterthan"));
	REQUIRE(found);
	CHECK(found->name == "GreaterThan");
	CHECK(FindAIProperty(properties, utils::hash_id("Radius")) == nullptr);
}
