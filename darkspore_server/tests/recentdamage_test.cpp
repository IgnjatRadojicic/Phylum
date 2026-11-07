#include <doctest/doctest.h>

#include "game/recentdamage.h"

using Game::RecentDamage;

TEST_CASE("recent damage adds up what one object took since a time") {
	RecentDamage damage;
	damage.Record(7, 10, 1000);
	damage.Record(7, 5, 2000);
	damage.Record(8, 50, 2000);

	CHECK(damage.Total(7, 0, 100) == doctest::Approx(15));
	// Only hits after the time count.
	CHECK(damage.Total(7, 1000, 100) == doctest::Approx(5));
	CHECK(damage.Total(9, 0, 100) == doctest::Approx(0));
}

TEST_CASE("recent damage stops at its cap") {
	RecentDamage damage;
	damage.Record(7, 30, 1000);
	damage.Record(7, 30, 1100);
	CHECK(damage.Total(7, 0, 40) == doctest::Approx(40));
}

TEST_CASE("recent damage keeps only the latest hits") {
	RecentDamage damage;
	damage.Record(7, 1000, 1);
	for (size_t i = 0; i < RecentDamage::capacity; ++i) {
		damage.Record(8, 1, 2);
	}
	CHECK(damage.Total(7, 0, 5000) == doctest::Approx(0));
}
