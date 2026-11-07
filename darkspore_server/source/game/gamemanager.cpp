
// Include
#include "gamemanager.h"
#include "raknet/server.h"

// Game
namespace Game {
	// GameManager
	std::map<uint32_t, InstancePtr> GameManager::sGames;
	std::map<std::string, Matchmaking> GameManager::sMatchmaking;

	InstancePtr GameManager::CreateGame() {
		// Never reused, so a new game cannot take the id of one the client was told is destroyed.
		static uint32_t nextId = 1;
		const uint32_t id = nextId++;

		auto game = Instance::Create(id);
		sGames[game->GetId()] = game;
		return game;
	}

	void GameManager::RemoveGame(uint32_t id) {
		auto it = sGames.find(id);
		if (it != sGames.end()) {
			// Stopped here, not when the last pointer goes: its server holds the game port until it
			// stops, and the next game cannot start without it. Never call this from the game's own thread.
			it->second->Stop();
			sGames.erase(it);
		}
	}

	InstancePtr GameManager::GetGame(uint32_t id) {
		auto it = sGames.find(id);
		return (it != sGames.end()) ? it->second : nullptr;
	}

	void GameManager::StartGame(uint32_t id) {
		auto game = GetGame(id);
		if (game) {
			game->Start();
		} else {
			// Game doesn't exist.
		}
	}

	Matchmaking& GameManager::StartMatchmaking() {
		return sMatchmaking["test"];
	}
}
