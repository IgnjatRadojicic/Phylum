
// Include
#include "scriptcheck.h"

#include "gamemanager.h"
#include "instance.h"
#include "lua.h"

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

			-- Engine functions cannot take fake objects, so record calls to them instead.
			local stubbed = {}
			local safeTables = { nBit = true, nUtil = true }
			for tableName, value in pairs(_G) do
				local isEngineTable = tableName:match("^n%u") or tableName == "Locomotion" or tableName == "Ability"
				if type(value) == "table" and isEngineTable and not safeTables[tableName] then
					for name, field in pairs(value) do
						if type(field) == "function" and debug.getinfo(field, "S").what == "C" then
							local label = tableName .. "." .. name
							value[name] = function()
								stubbed[label] = (stubbed[label] or 0) + 1
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
			nThread.WaitUntilTime = function() end
			nThread.WaitForXSeconds = function() end

			local file = io.open("script-combat.txt", "w")
			local passed, total = 0, 0
			local names = {}
			for name in pairs(nAbility.scripts) do
				names[#names + 1] = name
			end
			table.sort(names)

			for _, name in ipairs(names) do
				local ability = nAbility.scripts[name]
				local basic = ability.descriptors and nBit.And(ability.descriptors, nDescriptors.IsBasic) ~= 0
				if basic and ability.tick then
					total = total + 1
					calls = { animation = 0, damage = 0 }
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
					local good = ok and calls.animation > 0 and calls.damage > 0
					if good then
						passed = passed + 1
					end
					file:write(string.format("%s %s animation=%d damage=%d %s\n",
						good and "PASS" or "FAIL", name, calls.animation, calls.damage, ok and "" or tostring(err)))
				end
			end

			file:write(string.format("Basic attacks passed: %d of %d\n", passed, total))
			for label, count in pairs(stubbed) do
				file:write("STUB ", label, " ", count, "\n")
			end
			file:close()
		)lua";
	}

	// ScriptCheck
	void ScriptCheck::WriteReport(const std::filesystem::path& reportPath) {
		std::ofstream report(reportPath);

		// Script errors go to the console, so send them to the report while the check runs.
		auto* consoleBuffer = std::cout.rdbuf(report.rdbuf());

		const auto game = GameManager::CreateGame();
		Lua lua(*game);
		lua.Initialize();
		lua.PreloadAbilities();

		std::filesystem::remove("script-combat.txt");

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
