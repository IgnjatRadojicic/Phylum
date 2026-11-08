#include <doctest/doctest.h>

#include "game/actionevents.h"

#include <vector>

using namespace Game;

namespace {
	struct Sent {
		uint32_t agent;
		AbilityEvent event;
	};

	class RecordingSink : public AbilityEventSink {
		public:
			void SendEvent(uint32_t agent, const AbilityEvent& event) override {
				sent.push_back({ agent, event });
			}

			std::vector<Sent> sent;
	};
}

TEST_CASE("using an ability tells the caster, with the ability's descriptors") {
	RecordingSink sink;
	ActionEvents events(sink);
	events.UsedAbility(7, 0x21);

	REQUIRE(sink.sent.size() == 1);
	CHECK(sink.sent[0].agent == 7);
	CHECK(sink.sent[0].event.GetType() == AbilityEventType::UsedAbility);
	CHECK(sink.sent[0].event.GetInt(2) == 0x21);
}

TEST_CASE("a death tells everyone near it who died and how far away they were") {
	RecordingSink sink;
	ActionEvents events(sink);
	events.NearbyDeath(5, { { 5, 0.f }, { 8, 12.5f } });

	REQUIRE(sink.sent.size() == 2);
	CHECK(sink.sent[0].agent == 5);
	CHECK(sink.sent[1].agent == 8);
	CHECK(sink.sent[1].event.GetType() == AbilityEventType::NearbyDeath);
	CHECK(sink.sent[1].event.GetGUID(1) == 5);
	CHECK(sink.sent[1].event.GetFloat(2) == doctest::Approx(12.5f));
}

TEST_CASE("starting an action, a teleport and a swap each tell that one object, with no slots") {
	RecordingSink sink;
	ActionEvents events(sink);
	events.StartedNewAction(7);
	events.BreakRoot(8);
	events.PlayerSwap(9);

	REQUIRE(sink.sent.size() == 3);
	CHECK(sink.sent[0].agent == 7);
	CHECK(sink.sent[0].event.GetType() == AbilityEventType::StartedNewAction);
	CHECK(sink.sent[1].agent == 8);
	CHECK(sink.sent[1].event.GetType() == AbilityEventType::BreakRoot);
	CHECK(sink.sent[2].agent == 9);
	CHECK(sink.sent[2].event.GetType() == AbilityEventType::PlayerSwap);
	CHECK(sink.sent[2].event.GetGUID(1) == 0);
}

TEST_CASE("losing the last modifier of a kind tells its agent which kind") {
	RecordingSink sink;
	ActionEvents events(sink);
	events.LostModifierOfType(7, 0xABCD1234);

	REQUIRE(sink.sent.size() == 1);
	CHECK(sink.sent[0].agent == 7);
	CHECK(sink.sent[0].event.GetType() == AbilityEventType::LostModifierOfType);
	CHECK(sink.sent[0].event.GetGUID(1) == 0xABCD1234);
}

TEST_CASE("events with no object reach no one") {
	RecordingSink sink;
	ActionEvents events(sink);
	events.UsedAbility(0, 1);
	events.NearbyDeath(0, { { 8, 1.f } });
	events.StartedNewAction(0);
	events.BreakRoot(0);
	events.LostModifierOfType(0, 5);
	events.PlayerSwap(0);
	CHECK(sink.sent.empty());
}
