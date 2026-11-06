
#include <doctest/doctest.h>

#include "game/damageevents.h"

#include <functional>
#include <vector>

using namespace Game;

namespace {
	struct Sent {
		uint32_t agent;
		AbilityEvent event;
	};

	// Records what would reach the scripts, and can act like a handler that deals damage.
	class RecordingSink : public AbilityEventSink {
		public:
			void SendEvent(uint32_t agent, const AbilityEvent& event) override {
				sent.push_back({ agent, event });
				if (onEvent) {
					onEvent();
				}
			}

			std::vector<Sent> sent;
			std::function<void()> onEvent;
	};

	DamageReport Hit() {
		DamageReport hit;
		hit.victim = 10;
		hit.attacker = 20;
		hit.amount = 37.5f;
		hit.descriptors = 0x40;
		hit.damageType = 3;
		hit.damageSource = 1;
		hit.sourceModifier = 0xABCD;
		hit.ability = 0x1234;
		return hit;
	}
}

TEST_CASE("a hit tells the victim it took damage, with the attacker as the other side") {
	RecordingSink sink;
	DamageEvents events(sink);
	events.Report(Hit());

	REQUIRE(sink.sent.size() == 2);
	const auto& took = sink.sent[0];
	CHECK(took.agent == 10);
	CHECK(took.event.GetType() == AbilityEventType::TookDamage);
	CHECK(took.event.GetGUID(1) == 20);
	CHECK(took.event.GetFloat(2) == 37.5f);
	CHECK(took.event.GetInt(3) == 0x40);
	CHECK(took.event.GetInt(4) == 3);
	CHECK(took.event.GetInt(5) == 1);
	CHECK(took.event.GetGUID(6) == 0xABCD);
	CHECK(took.event.GetGUID(7) == 0x1234);
}

TEST_CASE("a hit tells the attacker it dealt damage, with the victim as the other side") {
	RecordingSink sink;
	DamageEvents events(sink);
	events.Report(Hit());

	REQUIRE(sink.sent.size() == 2);
	const auto& dealt = sink.sent[1];
	CHECK(dealt.agent == 20);
	CHECK(dealt.event.GetType() == AbilityEventType::DealtDamage);
	CHECK(dealt.event.GetGUID(1) == 10);
	CHECK(dealt.event.GetFloat(2) == 37.5f);
}

TEST_CASE("a hit with no known attacker only reaches the victim") {
	RecordingSink sink;
	DamageEvents events(sink);
	auto hit = Hit();
	hit.attacker = 0;
	events.Report(hit);

	REQUIRE(sink.sent.size() == 1);
	CHECK(sink.sent[0].event.GetType() == AbilityEventType::TookDamage);
	CHECK(sink.sent[0].event.GetGUID(1) == 0);
}

TEST_CASE("a hit that did no damage raises no events") {
	RecordingSink sink;
	DamageEvents events(sink);
	auto hit = Hit();
	hit.amount = 0.f;
	events.Report(hit);

	CHECK(sink.sent.empty());
}

TEST_CASE("a hit dealt by a damage handler raises no events of its own") {
	RecordingSink sink;
	DamageEvents events(sink);

	// Like thorns: the victim's handler hits the attacker back.
	sink.onEvent = [&] {
		auto reflected = Hit();
		reflected.victim = 20;
		reflected.attacker = 10;
		events.Report(reflected);
	};
	events.Report(Hit());

	CHECK(sink.sent.size() == 2);

	// The guard is lifted afterwards, so the next hit is reported again.
	sink.onEvent = nullptr;
	events.Report(Hit());
	CHECK(sink.sent.size() == 4);
}

TEST_CASE("event slots count from one, and unset or unknown slots read as zero") {
	AbilityEvent event(AbilityEventType::TookDamage);
	event.SetGUID(1, 5).SetGUID(0, 9).SetGUID(AbilityEvent::SlotCount + 1, 9);

	CHECK(event.GetGUID(1) == 5);
	CHECK(event.GetGUID(0) == 0);
	CHECK(event.GetGUID(2) == 0);
	CHECK(event.GetGUID(AbilityEvent::SlotCount + 1) == 0);
	CHECK(event.GetFloat(3) == 0.f);
	CHECK(event.GetInt(4) == 0);
}
