
// Include
#include "projectileflightcheck.h"

#include "instance.h"
#include "lua.h"
#include "object.h"
#include "objectmanager.h"

#include "utils/functions.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {
	// Follows the loop the game's own projectile script uses: wait, take a hit, and go on with
	// the range left if the shot pierces.
	constexpr const char* fireShot = R"lua(
		function FlightCheckLob(agent, projectile, x, y, z, height, flightTime, landing)
			nThread.Create(projectile, function()
				landing.x, landing.y, landing.z = nThread.WaitForLobbedProjectile(agent, projectile, x, y, z, height, flightTime)
				landing.done = true
			end)
		end

		function FlightCheckFire(agent, projectile, piercing, speed, range, hits)
			nThread.Create(projectile, function()
				local remaining = range
				repeat
					local hit, hitGround, x, y, z, left = nThread.WaitForProjectile(agent, projectile, {
						mDirection = { 1, 0, 0 },
						mSpeed = speed,
						mRange = remaining,
						mPiercing = piercing
					})
					if hit ~= kObjIDNone then
						hits[#hits + 1] = hit
					end
					remaining = left
				until hit == kObjIDNone or not piercing
				hits.done = true
			end)
		end
	)lua";

	constexpr float tickSeconds = 0.05f;
	constexpr int maximumTicks = 600;
	constexpr float laneSpacing = 100.f;
	constexpr uint8_t shooterTeam = 1;
	constexpr uint8_t enemyTeam = 2;

	struct Mob {
		glm::vec2 position;
		uint8_t team = enemyTeam;
	};

	struct Shot {
		std::string name;
		bool piercing = false;
		float speed = 20;
		float range = 30;
		std::vector<Mob> mobs;
		// Indexes into mobs, in the order they should be hit.
		std::vector<size_t> expected;
	};

	std::vector<Shot> GetShots() {
		const glm::vec2 stack(10, 0);
		return {
			{ "stacked-normal-hits-one", false, 20, 30, { { stack }, { stack }, { stack } }, { 0 } },
			{ "stacked-piercing-hits-all-once", true, 20, 30, { { stack }, { stack }, { stack } }, { 0, 1, 2 } },
			{ "line-piercing-nearest-first", true, 20, 30, { { { 21, 0 } }, { { 7, 0 } }, { { 14, 0 } } }, { 1, 2, 0 } },
			{ "nearest-wins-for-normal-shot", false, 20, 30, { { { 21, 0 } }, { { 7, 0 } } }, { 1 } },
			{ "fast-shot-cannot-skip-a-mob", false, 600, 90, { { { 10, 0 } } }, { 0 } },
			{ "mob-beside-path-is-missed", false, 20, 30, { { { 10, 20 } } }, {} },
			{ "mob-past-range-is-missed", false, 20, 8, { { { 25, 0 } } }, {} },
			{ "ally-is-not-hit", false, 20, 30, { { stack, shooterTeam } }, {} }
		};
	}
}

// Game
namespace Game {
	// ProjectileFlightCheck
	void ProjectileFlightCheck::Run(Instance& game, Lua& lua, const std::filesystem::path& reportPath) {
		std::ofstream report(reportPath);

		lua.LoadBuffer(fireShot);
		sol::state_view state(lua.GetState());
		sol::protected_function fire = state["FlightCheckFire"];

		auto& objects = game.GetObjectManager();
		const uint32_t mobNoun = utils::hash_id("ZelemBasicHybrid.Noun");
		const uint32_t projectileNoun = utils::hash_id("Ability_Fireball.Noun");

		// The game's own script, from the cast to the hit. Its waits follow the real clock.
		float scriptLane = -500;
		for (const char* abilityName : { "Fireball", "Frostbolt", "HomingFireball", "FireToss" }) {
			scriptLane -= laneSpacing;
			const auto& ability = lua.GetAbility(abilityName);
			if (!ability) {
				report << "FLIGHT script-" << abilityName << " wrong not found\n";
				continue;
			}

			auto caster = objects.Create(mobNoun);
			auto target = objects.Create(mobNoun);
			caster->SetPosition(glm::vec3(0, scriptLane, 0));
			caster->SetTeam(shooterTeam);
			caster->SetAttributeValue(AttributeType::CooldownScale, 1);
			target->SetPosition(glm::vec3(10, scriptLane, 0));
			target->SetTeam(enemyTeam);

			const float startingHealth = target->GetHealth();
			const bool cast = ability->Tick(caster, target, target->GetPosition(), 1);
			const bool castBlocked = !ability->Tick(caster, target, target->GetPosition(), 1);
			for (int ticks = 0; ticks < 300 && target->GetHealth() >= startingHealth; ++ticks) {
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				game.Update();
			}

			const bool good = cast && castBlocked && target->GetHealth() < startingHealth;
			report << "FLIGHT script-" << abilityName << (good ? " good" : " wrong") << " secondCastBlocked=" << castBlocked
				<< " health=" << startingHealth << "->" << target->GetHealth() << "\n";
		}

		// A real lob through the adapter: lands where it was aimed, after its flight time.
		{
			auto thrower = objects.Create(mobNoun);
			auto shell = objects.Create(projectileNoun);
			thrower->SetPosition(glm::vec3(0, -2000, 0));
			shell->SetPosition(glm::vec3(0, -2000, 0));

			sol::table landing = state.create_table();
			sol::protected_function lob = state["FlightCheckLob"];
			auto lobbed = lob(thrower->GetId(), shell->GetId(), 12, -2000, 0, 4, 1, landing);
			if (!lobbed.valid()) {
				sol::error error = lobbed;
				report << "FLIGHT lob-lands-on-target wrong " << error.what() << "\n";
			} else {
				int ticks = 0;
				float highest = 0;
				for (; ticks < maximumTicks && !landing.get_or("done", false); ++ticks) {
					objects.Update(tickSeconds);
					lua.Update();
					highest = std::max(highest, shell->GetPosition().z);
				}

				const float landedX = landing.get_or("x", 0.f);
				const bool good = landing.get_or("done", false) && landedX == 12 && highest > 3.5f && highest < 4.5f && ticks >= 19 && ticks <= 22;
				report << "FLIGHT lob-lands-on-target " << (good ? "good" : "wrong") << " ticks=" << ticks
					<< " landedX=" << landedX << " highest=" << highest << "\n";
			}
		}

		float lane = 0;
		for (const auto& shot : GetShots()) {
			lane += laneSpacing;

			const auto spawn = [&](uint32_t noun, const glm::vec2& at, uint8_t team) {
				auto object = objects.Create(noun);
				if (object) {
					object->SetPosition(glm::vec3(at.x, at.y + lane, 0));
					object->SetTeam(team);
				}
				return object;
			};

			auto shooter = spawn(mobNoun, { 0, 0 }, shooterTeam);
			auto projectile = spawn(projectileNoun, { 0, 0 }, shooterTeam);
			if (!shooter || !projectile || !projectile->HasLocomotionData()) {
				report << "FLIGHT " << shot.name << " wrong could not create objects\n";
				continue;
			}
			projectile->SetOwnerObject(shooter);

			std::vector<uint32_t> expected;
			std::vector<uint32_t> mobIds;
			for (const auto& mob : shot.mobs) {
				auto object = spawn(mobNoun, mob.position, mob.team);
				mobIds.push_back(object ? object->GetId() : 0);
			}
			for (size_t index : shot.expected) {
				expected.push_back(mobIds[index]);
			}

			sol::table hits = state.create_table();
			auto fired = fire(shooter->GetId(), projectile->GetId(), shot.piercing, shot.speed, shot.range, hits);
			if (!fired.valid()) {
				sol::error error = fired;
				report << "FLIGHT " << shot.name << " wrong " << error.what() << "\n";
				continue;
			}

			int ticks = 0;
			for (; ticks < maximumTicks && !hits.get_or("done", false); ++ticks) {
				objects.Update(tickSeconds);
				lua.Update();
			}

			std::vector<uint32_t> actual;
			for (size_t index = 1; index <= hits.size(); ++index) {
				actual.push_back(hits.get<uint32_t>(index));
			}

			const bool finished = hits.get_or("done", false);
			report << "FLIGHT " << shot.name << (finished && actual == expected ? " good" : " wrong")
				<< " ticks=" << ticks << " finished=" << finished << " stoppedAtX=" << projectile->GetPosition().x << " hits=";
			for (uint32_t id : actual) {
				report << id << " ";
			}
			report << "expected=";
			for (uint32_t id : expected) {
				report << id << " ";
			}
			report << "\n";
		}
	}
}
