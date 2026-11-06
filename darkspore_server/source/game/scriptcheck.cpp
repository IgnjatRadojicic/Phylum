
// Include
#include "scriptcheck.h"

#include "gamemanager.h"
#include "instance.h"
#include "lua.h"
#include "projectileflightcheck.h"

#include <fstream>
#include <iostream>

// Game
namespace Game {
	namespace {
		// What scripts can really reach, which a text search of the source cannot tell.
		constexpr const char* listApi = R"lua(
			local names = {}
			for tableName, value in pairs(_G) do
				if type(value) == "table" and tableName:match("^n%u") then
					for name, field in pairs(value) do
						if type(name) == "string" and type(field) == "function" then
							names[#names + 1] = tableName .. "." .. name
						end
					end
				end
			end
			table.sort(names)
			local file = io.open("script-api.txt", "w")
			file:write(table.concat(names, "\n"), "\n")
			file:close()
		)lua";

		// Runs each basic attack's real script against fake objects, so the attack logic can be
		// checked with no game client. Results go to script-combat.txt.
		constexpr const char* testBasicAttacks = R"lua(
			local calls
			local function fakeObject(id, team, x)
				local object = {}
				function object:GetId() return id end
				function object:GetPosition() return { x = x, y = 0, z = 0 } end
				function object:GetFacing() return { x = 1, y = 0, z = 0 } end
				function object:GetCenterPoint() return { x = x, y = 0, z = 1 } end
				function object:GetOrientation() return { x = 0, y = 0, z = 0, w = 1 } end
				function object:SetOrientation() end
				function object:SetOwnerObject() end
				function object:MarkForDeletion() end
				function object:GetSharedAbilityOffset() return 0 end
				function object:SetTeam() end
				function object:SetStealthType() end
				function object:SetAttributeSnapshot() end
				function object:AddEffect() return 1 end
				function object:GetTeam() return team end
				function object:GetHealth() return 100 end
				function object:GetMaxHealth() return 100 end
				function object:GetOwnerObject() return nil end
				function object:GetNpcType() return 0 end
				function object:GetFootprintRadius() return 1 end
				function object:GetWeaponDamage() return { 6, 12 } end
				function object:GetAttributeSnapshot() return {} end
				function object:GetAttributeValue() return 0 end
				function object:IsPlayerControlled() return team == 1 end
				function object:SetAnimationState() calls.animation = calls.animation + 1 end
				function object:TakeDamage()
					calls.damage = calls.damage + 1
					return true, 9, false
				end
				return object
			end

			-- A quarter turn about Z takes X to Y. A wrong argument order gives no error, only
			-- shots in the wrong place, so test the real adapter before stubs replace it.
			local half = math.sqrt(0.5)
			local turnedX, turnedY = nMathUtil.TransformVector(1, 0, 0, 0, 0, half, half)
			local mathGood = math.abs(turnedX) < 0.001 and math.abs(turnedY - 1) < 0.001

			-- A failure usually comes from a nil handed back earlier. A stub is a limit of this
			-- check, and a missing name is a real gap, so the trail keeps them apart.
			local trail = {}
			local missing = {}
			local recordGap = ScriptGaps.Record
			ScriptGaps.Record = function(name)
				missing[name] = (missing[name] or 0) + 1
				trail[#trail + 1] = "missing:" .. name
				recordGap(name)
			end

			-- Engine functions cannot take fake objects, so record calls to them instead.
			local stubbed = {}
			local safeTables = { nBit = true, nUtil = true }
			local adapterTables = { nLocomotion = true, nObjectManager = true, nMathUtil = true }
			for tableName, value in pairs(_G) do
				local isEngineTable = tableName:match("^n%u") or tableName == "Locomotion" or tableName == "Ability"
				if type(value) == "table" and isEngineTable and not safeTables[tableName] then
					for name, field in pairs(value) do
						if type(field) == "function" and (adapterTables[tableName] or debug.getinfo(field, "S").what == "C") then
							local label = tableName .. "." .. name
							value[name] = function()
								stubbed[label] = (stubbed[label] or 0) + 1
								trail[#trail + 1] = "stub:" .. label
							end
						end
					end
				end
			end

			local agent = fakeObject(1, 1, 0)
			local target = fakeObject(2, 2, 1)
			nObjectManager.GetObject = function(id)
				return (id == 1 and agent) or (id == 2 and target) or nil
			end
			nObjectManager.GetObjectsInRadius = function() return { 2 } end
			nObjectManager.GetObjectsInRadius_SortedByDistance = function() return { 2 } end
			nObjectManager.CreateObject = function() return 2 end
			nMathUtil.RotateVectorByAxisAngle = function(x, y, z) return x, y, z end
			nMathUtil.TransformVector = function(x, y, z) return x, y, z end
			nMathUtil.CircleIntersectsArc = function() return true end
			nThread.WaitUntilTime = function() end
			nThread.WaitForXSeconds = function() end

			-- Scripts run the launch as a thread. Here it runs at once, inside the ability.
			nThread.Create = function(_, callback, ...)
				callback(...)
			end

			nThread.EngineWaitForLobbedProjectile = function(_, destination)
				return { x = destination.x, y = destination.y, z = destination.z }
			end

			-- Answers as the engine does: one hit, then the shot runs out of range. A script that
			-- keeps waiting after that would hang a real projectile thread, so it fails here.
			local projectileWaits = 0
			nThread.EngineWaitForProjectile = function()
				projectileWaits = projectileWaits + 1
				calls.projectile = calls.projectile + 1
				if projectileWaits > 20 then
					error("projectile loop never ended")
				end
				if projectileWaits == 1 then
					return 2, false, { x = 1, y = 0, z = 0 }, 5
				end
				return 0, false, { x = 2, y = 0, z = 0 }, 0
			end

			local file = io.open("script-combat.txt", "w")
			file:write("MATH ", mathGood and "good" or "wrong", "\n")
			local passed, total = 0, 0
			local names = {}
			for name in pairs(nAbility.scripts) do
				names[#names + 1] = name
			end
			table.sort(names)

			for _, name in ipairs(names) do
				local ability = nAbility.scripts[name]
				local basic = ability.descriptors and nBit.And(ability.descriptors, nDescriptors.IsBasic) ~= 0
				if ability.tick then
					total = total + 1
					calls = { animation = 0, damage = 0, projectile = 0 }
					trail = {}
					projectileWaits = 0
					file:write("RUN ", name, "\n")
					file:flush()
					local thread = coroutine.create(function()
						ability.tick(ability, agent, target, { x = 1, y = 0, z = 0 }, 1)
					end)
					-- A crash in native code cannot be caught, so leave a trail of what was called.
					debug.sethook(thread, function()
						local info = debug.getinfo(2, "nS")
						if info.what == "C" then
							file:write("CALL ", tostring(info.name), "\n")
							file:flush()
						end
					end, "c")
					local ok, err = coroutine.resume(thread)
					local good = ok and (calls.animation > 0 or calls.damage > 0)
					if good then
						passed = passed + 1
					end
					file:write(string.format("%s %s %s animation=%d damage=%d %s\n",
						good and "PASS" or (ok and "IDLE" or "FAIL"), basic and "basic" or "ability", name, calls.animation, calls.damage, ok and "" or tostring(err)))
					if not ok then
						file:write("TRAIL ", name, " ", table.concat(trail, " ", math.max(1, #trail - 3)), "\n")
					end
				end
			end

			file:write(string.format("Abilities passed: %d of %d\n", passed, total))
			for label, count in pairs(stubbed) do
				file:write("STUB ", label, " ", count, "\n")
			end
			for label, count in pairs(missing) do
				file:write("MISSING ", label, " ", count, "\n")
			end
			file:close()
		)lua";
	}

	// ScriptCheck
	void ScriptCheck::WriteReport(const std::filesystem::path& reportPath) {
		std::ofstream report(reportPath);

		// The check has its own file, so a run never mixes with what a live server logged.
		constexpr const char* gapLogPath = "script-check-missing.txt";
		std::filesystem::remove(gapLogPath);

		// Script errors go to the console, so send them to the report while the check runs.
		auto* consoleBuffer = std::cout.rdbuf(report.rdbuf());

		const auto game = GameManager::CreateGame();
		game->CreateWorld();
		Lua& lua = game->GetLua();
		lua.SetGapLogPath(gapLogPath);
		lua.Initialize();
		lua.PreloadAbilities();

		std::filesystem::remove("script-combat.txt");

		// Runs first, because the ability test below replaces engine functions with stubs.
		ProjectileFlightCheck::Run(*game, lua, "script-flight.txt");

		for (const char* check : { listApi, testBasicAttacks }) {
			sol::protected_function_result result = lua.LoadBuffer(check);
			if (!result.valid()) {
				sol::error err = result;
				std::cout << "Script check failed: " << err.what() << std::endl;
			}
		}

		std::cout.rdbuf(consoleBuffer);
	}
}
