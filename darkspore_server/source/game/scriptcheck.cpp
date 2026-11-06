
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

		std::cout.rdbuf(consoleBuffer);
	}
}
