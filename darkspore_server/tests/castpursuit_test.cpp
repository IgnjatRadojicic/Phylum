#include <doctest/doctest.h>

#include "game/castpursuit.h"

using Game::CastPursuit;

namespace {
	CastPursuit::Cast Far() {
		CastPursuit::Cast cast;
		cast.casterId = 2;
		cast.abilityId = 3176239913;
		cast.targetId = 9;
		cast.shouldPursue = true;
		cast.range = 12;
		cast.gap = 18;
		return cast;
	}
}

TEST_CASE("a pursuing ability at a target out of range waits, and its retry on arrival runs") {
	CastPursuit pursuit;
	CHECK(pursuit.Decide(Far(), 1000).decision == CastPursuit::Decision::Pursue);
	auto arrived = Far();
	arrived.targetId = 0;
	arrived.gap = 11;
	CHECK(pursuit.Decide(arrived, 2500).decision == CastPursuit::Decision::Run);
}

TEST_CASE("the arrival runs even if the target moved away again, so a chase cannot loop") {
	CastPursuit pursuit;
	CHECK(pursuit.Decide(Far(), 1000).decision == CastPursuit::Decision::Pursue);
	auto arrived = Far();
	arrived.targetId = 0;
	CHECK(pursuit.Decide(arrived, 1200).decision == CastPursuit::Decision::Run);
	// Only once: the next far cast is a new chase.
	CHECK(pursuit.Decide(Far(), 1400).decision == CastPursuit::Decision::Pursue);
}

TEST_CASE("casting at the chased target again while still far keeps pursuing, it does not fire") {
	CastPursuit pursuit;
	CHECK(pursuit.Decide(Far(), 1000).decision == CastPursuit::Decision::Pursue);
	CHECK(pursuit.Decide(Far(), 1100).decision == CastPursuit::Decision::Pursue);
	auto near = Far();
	near.gap = 9;
	CHECK(pursuit.Decide(near, 1200).decision == CastPursuit::Decision::Run);
}

TEST_CASE("casts in range, without a target, without pursuit or without a range run at once") {
	CastPursuit pursuit;
	auto near = Far();
	near.gap = 12;
	CHECK(pursuit.Decide(near, 0).decision == CastPursuit::Decision::Run);
	auto ground = Far();
	ground.targetId = 0;
	CHECK(pursuit.Decide(ground, 0).decision == CastPursuit::Decision::Run);
	auto still = Far();
	still.shouldPursue = false;
	CHECK(pursuit.Decide(still, 0).decision == CastPursuit::Decision::Run);
	auto unranged = Far();
	unranged.range = 0;
	CHECK(pursuit.Decide(unranged, 0).decision == CastPursuit::Decision::Run);
}

TEST_CASE("another cast, another target, a timeout or a forget ends the chase") {
	CastPursuit pursuit;
	auto other = Far();
	other.abilityId = 1;

	pursuit.Decide(Far(), 0);
	CHECK(pursuit.Decide(other, 100).decision == CastPursuit::Decision::Pursue);
	CHECK(pursuit.Decide(Far(), 200).decision == CastPursuit::Decision::Pursue);

	auto otherTarget = Far();
	otherTarget.targetId = 10;
	CHECK(pursuit.Decide(otherTarget, 300).decision == CastPursuit::Decision::Pursue);

	CHECK(pursuit.Decide(Far(), 400).decision == CastPursuit::Decision::Pursue);
	CHECK(pursuit.Decide(Far(), 400 + CastPursuit::timeoutMs + 1).decision == CastPursuit::Decision::Pursue);

	pursuit.Forget(2);
	CHECK(pursuit.Decide(Far(), 400 + CastPursuit::timeoutMs + 2).decision == CastPursuit::Decision::Pursue);
}

TEST_CASE("a retry that comes without a target runs at the chased one") {
	CastPursuit pursuit;
	CHECK(pursuit.Decide(Far(), 0).decision == CastPursuit::Decision::Pursue);
	auto arrived = Far();
	arrived.targetId = 0;
	const auto answer = pursuit.Decide(arrived, 200);
	CHECK(answer.decision == CastPursuit::Decision::Run);
	CHECK(answer.targetId == 9);

	// With no chase, a cast without a target stays without one.
	CHECK(pursuit.Decide(arrived, 300).targetId == 0);
}

TEST_CASE("a repeat cast at the chased target just past its range runs, further away it pursues") {
	CastPursuit pursuit;
	CHECK(pursuit.Decide(Far(), 0).decision == CastPursuit::Decision::Pursue);
	auto justShort = Far();
	justShort.gap = 12.5f;
	CHECK(pursuit.Decide(justShort, 100).decision == CastPursuit::Decision::Run);

	CHECK(pursuit.Decide(Far(), 200).decision == CastPursuit::Decision::Pursue);
	auto stillFar = Far();
	stillFar.gap = 14;
	CHECK(pursuit.Decide(stillFar, 300).decision == CastPursuit::Decision::Pursue);
}
