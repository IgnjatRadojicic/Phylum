
#include <doctest/doctest.h>

#include "game/scriptgaps.h"

#include <sstream>

TEST_CASE("a missing name is logged the first time only") {
	std::ostringstream log;
	Game::ScriptGaps gaps(log);

	CHECK(gaps.Record("nModifier.AddAttributeModifier"));
	CHECK_FALSE(gaps.Record("nModifier.AddAttributeModifier"));
	CHECK(log.str() == "nModifier.AddAttributeModifier\n");
}

TEST_CASE("different missing names are all logged") {
	std::ostringstream log;
	Game::ScriptGaps gaps(log);

	gaps.Record("nThread.CreateThreadForObject");
	gaps.Record("global.kObjIDNone");

	CHECK(log.str() == "nThread.CreateThreadForObject\nglobal.kObjIDNone\n");
}
