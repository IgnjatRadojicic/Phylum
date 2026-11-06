
#include <doctest/doctest.h>

#include "game/difficultytuning.h"

#include <pugixml.hpp>

using namespace Game;

TEST_CASE("rating conversion is read from the game's nested entries") {
	pugi::xml_document document;
	REQUIRE(document.load_string(R"(
		<difficultytuning>
			<HealthPercentIncrease><entry><entry>99.0</entry></entry></HealthPercentIncrease>
			<RatingConversion>
				<entry><entry>10.00000</entry></entry>
				<entry><entry>10.53000</entry></entry>
				<entry><entry>11.11000</entry></entry>
			</RatingConversion>
		</difficultytuning>)"));

	const auto tuning = DifficultyTuning::FromXml(document.child("difficultytuning"));
	CHECK(tuning.GetRatingConversion(1) == doctest::Approx(10.f));
	CHECK(tuning.GetRatingConversion(2) == doctest::Approx(10.53f));
	CHECK(tuning.GetRatingConversion(3) == doctest::Approx(11.11f));
}

TEST_CASE("rating conversion outside the table gives the nearest end") {
	const DifficultyTuning tuning({ 10.f, 20.f, 30.f });
	CHECK(tuning.GetRatingConversion(0) == 10.f);
	CHECK(tuning.GetRatingConversion(99) == 30.f);
}

TEST_CASE("rating conversion with no table is zero") {
	const DifficultyTuning tuning;
	CHECK(tuning.GetRatingConversion(1) == 0.f);
}
