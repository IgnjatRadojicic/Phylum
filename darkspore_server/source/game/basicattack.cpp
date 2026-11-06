
// Include
#include "basicattack.h"

#include "utils/functions.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Game
namespace Game {
	namespace {
		using Sequence = std::vector<BasicAttackStep>;

		std::string ReadQuoted(const std::string& text, size_t start) {
			const auto open = text.find('"', start);
			if (open == std::string::npos) {
				return {};
			}

			const auto close = text.find('"', open + 1);
			return close == std::string::npos ? std::string {} : text.substr(open + 1, close - open - 1);
		}

		Sequence ReadSequence(const std::string& text, size_t start) {
			const auto end = text.find("selectionMethod", start);

			Sequence sequence;
			auto position = text.find("hit =", start);
			while (position != std::string::npos && position < end) {
				const float seconds = std::strtof(text.c_str() + position + 5, nullptr);

				const auto animation = text.find("PreloadAnimation(", position);
				if (animation == std::string::npos || animation >= end) {
					break;
				}

				BasicAttackStep step;
				step.animation = utils::hash_id(ReadQuoted(text, animation));
				step.hitDelay = static_cast<uint32_t>(seconds * 1000);
				sequence.push_back(step);

				position = text.find("hit =", animation);
			}
			return sequence;
		}

		std::unordered_map<uint32_t, Sequence> LoadSequences() {
			std::unordered_map<uint32_t, Sequence> sequences;

			const std::filesystem::path dataPath = "data/serverdata/Abilities/";
			if (!std::filesystem::exists(dataPath)) {
				return sequences;
			}

			for (const auto& entry : std::filesystem::directory_iterator(dataPath)) {
				if (!entry.is_regular_file() || entry.path().extension() != ".lua") {
					continue;
				}

				std::ifstream file(entry.path());
				std::stringstream buffer;
				buffer << file.rdbuf();

				const std::string text = buffer.str();
				const auto nameField = text.find(".abilityName =");
				const auto sequenceField = text.find(".animationSequence =");
				if (nameField == std::string::npos || sequenceField == std::string::npos || text.find("nDescriptors.IsBasic") == std::string::npos) {
					continue;
				}

				auto sequence = ReadSequence(text, sequenceField);
				if (!sequence.empty()) {
					sequences.try_emplace(utils::hash_id(ReadQuoted(text, nameField)), std::move(sequence));
				}
			}

			std::cout << "Loaded " << sequences.size() << " basic attack animation sequences." << std::endl;
			return sequences;
		}
	}

	// BasicAttacks
	const BasicAttackStep* BasicAttacks::NextStep(uint32_t abilityId, uint32_t objectId) {
		static const auto sequences = LoadSequences();
		static std::unordered_map<uint32_t, size_t> nextSteps;
		static std::mutex nextStepsMutex;

		const auto it = sequences.find(abilityId);
		if (it == sequences.end()) {
			return nullptr;
		}

		std::lock_guard lock(nextStepsMutex);
		auto& nextStep = nextSteps[objectId];
		const auto* step = &it->second[nextStep % it->second.size()];
		++nextStep;
		return step;
	}
}
