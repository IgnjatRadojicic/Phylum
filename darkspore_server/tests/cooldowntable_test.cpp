#include <doctest/doctest.h>

#include "game/cooldowntable.h"

using Game::CooldownTable;

TEST_CASE("a started cooldown runs from now for its length") {
	CooldownTable table;
	const auto span = table.Start(7, 1000, 5000);
	CHECK(span.start == 1000);
	CHECK(span.end == 6000);
	CHECK(span.Duration() == 5000);
	CHECK(table.IsCooling(7, 5999));
	CHECK_FALSE(table.IsCooling(7, 6000));
	CHECK_FALSE(table.IsCooling(8, 1000));
}

TEST_CASE("shifting moves the end, only for an ability with a cooldown") {
	CooldownTable table;
	CHECK_FALSE(table.Shift(7, 1000));

	table.Start(7, 0, 5000);
	CHECK(table.Shift(7, -1000)->end == 4000);
	CHECK(table.Shift(7, 2500)->end == 6500);
	// Far enough back, the cooldown is simply over.
	CHECK(table.Shift(7, -10000)->end == 0);
}

TEST_CASE("clearing ends one cooldown now, or every one") {
	CooldownTable table;
	table.Start(1, 0, 5000);
	table.Start(2, 0, 8000);

	CHECK(table.Clear(1, 2000)->end == 2000);
	CHECK_FALSE(table.IsCooling(1, 2000));
	CHECK(table.IsCooling(2, 2000));

	const auto changes = table.ClearAll(3000);
	CHECK(changes.size() == 2);
	CHECK_FALSE(table.IsCooling(2, 3000));
	CHECK_FALSE(table.Clear(9, 3000));
}

TEST_CASE("scaling keeps that share of what is left of a running cooldown") {
	CooldownTable table;
	table.Start(1, 0, 10000);
	table.Start(2, 0, 1000);

	// 6000 left at 4000, a quarter of it stays: 1500.
	CHECK(table.Scale(1, 4000, 0.25f)->end == 5500);
	// A finished cooldown is left alone.
	CHECK_FALSE(table.Scale(2, 4000, 0.5f));

	// Above 1 it lengthens: 1500 left becomes 3000.
	CHECK(table.ScaleAll(4000, 2.f).size() == 1);
	CHECK(table.Get(1)->end == 7000);
}
