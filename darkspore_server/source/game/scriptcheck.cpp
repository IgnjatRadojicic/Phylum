
// Include
#include "scriptcheck.h"

#include "gamemanager.h"
#include "instance.h"
#include "lua.h"

#include <fstream>
#include <iostream>

// Game
namespace Game {
	// ScriptCheck
	void ScriptCheck::WriteReport(const std::filesystem::path& reportPath) {
		std::ofstream report(reportPath);

		// Script errors go to the console, so send them to the report while the check runs.
		auto* consoleBuffer = std::cout.rdbuf(report.rdbuf());

		const auto game = GameManager::CreateGame();
		Lua lua(*game);
		lua.Initialize();
		lua.PreloadAbilities();

		// What scripts can really reach, which a text search of the source cannot tell.
		lua.LoadBuffer(R"lua(
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
		)lua");

		std::cout.rdbuf(consoleBuffer);
	}
}
