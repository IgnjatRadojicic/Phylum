
// Include
#include "modifiercheck.h"

#include "instance.h"
#include "lua.h"
#include "modifiersystem.h"
#include "object.h"
#include "objectmanager.h"

#include "sporenet/user.h"

#include "utils/functions.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
	// Requests run in a thread, as they do when an ability asks for a modifier.
	constexpr const char* helpers = R"lua(
		function ModifierCheckRequest(agent, initiator, guid, rank, result)
			nThread.Create(initiator, function()
				result.id = nModifier.RequestModifier(agent, initiator, guid, nil, rank)
			end)
		end

		function ModifierCheckImmunityDetails(agent, guid)
			local modifier = nModifier.registered[guid]
			return "number=" .. tostring(nAttributeType.ImmuneToSlow)
				.. " seen=" .. tostring(nAttribute.GetAttributeValue(agent, nAttributeType.ImmuneToSlow))
				.. " canUse=" .. tostring(modifier.CanUseModifier and modifier.CanUseModifier(agent, agent, 1))
				.. " descriptors=" .. tostring(modifier.descriptors)
		end

		function ModifierCheckFindStacking()
			for guid, modifier in pairs(nModifier.registered) do
				if modifier.activationType == nActivationType.Stacks and modifier[nAbilityFns.HandleEvent] then
					return guid
				end
			end
		end

		function ModifierCheckAll()
			local guids = {}
			for guid in pairs(nModifier.registered) do
				guids[#guids + 1] = guid
			end
			table.sort(guids)
			return guids
		end
	)lua";

	constexpr const char* mobNounName = "ZelemBasicHybrid.Noun";
	constexpr int longestSlowMs = 15000;
	constexpr float mobHealth = 100000;

	class Checker {
		public:
			Checker(Game::Instance& game, Game::Lua& lua, std::ostream& report) :
				mGame(game), mLua(lua), mReport(report), mState(lua.GetState()), mPackets(std::make_shared<std::vector<std::string>>()) {
				const auto packets = mPackets;
				mGame.StartOffline(std::make_shared<SporeNet::User>("modifier-check"), [packets](std::string_view packet) {
					packets->emplace_back(packet);
				});
			}

			void RunAll() {
				const auto slow = utils::hash_id("QuantumSlowModifier");
				CheckSlowAppliesAndExpires(slow);
				CheckImmunityRefuses(slow);
				CheckStacking();
				CheckDeathEndsIt(slow);
				CheckScriptCanEndIt(slow);
				CheckDestroyedAgentLeavesNothing(slow);
				RunEveryScript();
			}

		private:
			Game::ObjectPtr Spawn() {
				auto object = mGame.GetObjectManager().Create(utils::hash_id(mobNounName));
				if (object) {
					object->SetAttributeValue(Game::AttributeType::MaxHealth, mobHealth);
					object->SetHealth(mobHealth);
				}
				return object;
			}

			uint32_t Request(const Game::ObjectPtr& agent, const Game::ObjectPtr& initiator, uint32_t guid, int rank = 1) {
				sol::table result = mState.create_table();
				sol::protected_function request = mState["ModifierCheckRequest"];
				sol::protected_function_result called = request(agent->GetId(), initiator->GetId(), guid, rank, result);
				if (!called.valid()) {
					sol::error error = called;
					std::cout << "Modifier request failed: " << error.what() << std::endl;
					return 0;
				}
				return result.get_or("id", 0u);
			}

			void Wait(int milliseconds) {
				for (int waited = 0; waited < milliseconds; waited += 20) {
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					mGame.Update();
				}
			}

			bool Saw(std::string_view packet) const {
				return std::find(mPackets->begin(), mPackets->end(), packet) != mPackets->end();
			}

			void Report(const char* name, bool good, const std::string& details) {
				mReport << "MODIFIER " << name << (good ? " good " : " wrong ") << details << "\n";
			}

			const Game::ModifierManager& Manager() {
				return mGame.GetModifiers().GetManager();
			}

			void CheckSlowAppliesAndExpires(uint32_t slow) {
				using Game::AttributeType;

				auto agent = Spawn();
				auto caster = Spawn();
				const float before = agent->GetAttributeValue(AttributeType::MovementSpeedBuff);

				mPackets->clear();
				const auto id = Request(agent, caster, slow);
				const float during = agent->GetAttributeValue(AttributeType::MovementSpeedBuff);
				const auto remaining = Manager().GetRemainingMs(id, mGame.GetTime());
				Report("slow-lowers-speed", id != 0 && during < before && Saw("ModifierCreated"),
					"before=" + std::to_string(before) + " during=" + std::to_string(during) + " remainingMs=" + std::to_string(remaining));

				int waited = 0;
				for (; waited < longestSlowMs && Manager().Get(id); waited += 100) {
					Wait(100);
				}

				const float after = agent->GetAttributeValue(AttributeType::MovementSpeedBuff);
				Report("slow-expires-and-restores", !Manager().Get(id) && after == before && Saw("ModifierDeleted"),
					"after=" + std::to_string(after) + " waitedMs=" + std::to_string(waited));
			}

			void CheckImmunityRefuses(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();
				agent->SetAttributeValue(Game::AttributeType::ImmuneToSlow, 1);

				const float before = agent->GetAttributeValue(Game::AttributeType::MovementSpeedBuff);

				sol::protected_function describe = mState["ModifierCheckImmunityDetails"];
				const std::string details = describe(agent->GetId(), slow).get<std::string>();

				Request(agent, caster, slow);

				const auto running = Manager().GetAll(agent->GetId()).size();
				const float after = agent->GetAttributeValue(Game::AttributeType::MovementSpeedBuff);
				Report("immunity-refuses-slow", running == 0 && after == before,
					"running=" + std::to_string(running) + " after=" + std::to_string(after) + " " + details);
			}

			void CheckStacking() {
				sol::protected_function find = mState["ModifierCheckFindStacking"];
				sol::optional<uint32_t> guid = find();
				if (!guid) {
					Report("stacks-through-script", false, "no stacking modifier found");
					return;
				}

				auto agent = Spawn();
				auto caster = Spawn();

				mPackets->clear();
				const auto first = Request(agent, caster, *guid);
				const auto second = Request(agent, caster, *guid);

				const auto instance = Manager().Get(first);
				const auto stacks = instance ? instance->stackCount : 0;
				Report("stacks-through-script", first != 0 && first == second && stacks == 2 && Saw("ModifierUpdated"),
					"guid=" + std::to_string(*guid) + " stacks=" + std::to_string(stacks));
			}

			void CheckDeathEndsIt(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();
				const float before = agent->GetAttributeValue(Game::AttributeType::MovementSpeedBuff);

				const auto id = Request(agent, caster, slow);
				mGame.GetModifiers().OnAgentDeath(agent->GetId());
				mGame.GetModifiers().Update();

				Report("death-ends-modifier", id != 0 && !Manager().Get(id) &&
					agent->GetAttributeValue(Game::AttributeType::MovementSpeedBuff) == before, "");
			}

			void CheckScriptCanEndIt(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();

				const auto id = Request(agent, caster, slow);
				sol::protected_function mark = mState["nModifier"]["MarkForDelete"];
				mark(agent->GetId(), id);
				const bool stillThere = Manager().Get(id) != nullptr;
				mGame.GetModifiers().Update();

				Report("mark-for-delete-ends-next-update", id != 0 && stillThere && !Manager().Get(id), "");
			}

			void CheckDestroyedAgentLeavesNothing(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();

				const auto agentId = agent->GetId();
				const auto id = Request(agent, caster, slow);

				agent->MarkForDeletion();
				agent.reset();
				Wait(120);

				Report("destroyed-agent-leaves-nothing", id != 0 && !Manager().Get(id) && Manager().GetAll(agentId).empty(), "");
			}

			// Not a pass or a fail: this finds which scripts need engine functions we do not have yet.
			void RunEveryScript() {
				sol::protected_function all = mState["ModifierCheckAll"];
				sol::table guids = all();

				size_t started = 0;
				size_t refused = 0;
				std::vector<std::string> failures;

				for (const auto& [_, value] : guids) {
					const auto guid = value.as<uint32_t>();

					auto agent = Spawn();
					auto caster = Spawn();
					if (!agent || !caster) {
						continue;
					}

					// Errors from scripts go to the console.
					std::ostringstream output;
					auto* console = std::cout.rdbuf(output.rdbuf());

					const auto id = Request(agent, caster, guid);
					for (uint32_t running : Manager().GetAll(agent->GetId())) {
						mGame.GetModifiers().MarkForDelete(running);
					}
					mGame.GetModifiers().Update();

					std::cout.rdbuf(console);

					const auto text = output.str();
					if (!text.empty()) {
						failures.push_back(std::to_string(guid) + " " + text.substr(0, text.find_first_of("\r\n", text.find('\n') + 1)));
					} else if (id != 0) {
						++started;
					} else {
						++refused;
					}

					auto& objects = mGame.GetObjectManager();
					agent->MarkForDeletion();
					caster->MarkForDeletion();
					objects.Update(0.05f);
				}

				mReport << "SCRIPTS started=" << started << " refused=" << refused << " failed=" << failures.size() << "\n";
				for (const auto& failure : failures) {
					std::string line = failure;
					std::replace(line.begin(), line.end(), '\n', ' ');
					mReport << "FAILED " << line << "\n";
				}
			}

		private:
			Game::Instance& mGame;
			Game::Lua& mLua;
			std::ostream& mReport;
			sol::state_view mState;
			std::shared_ptr<std::vector<std::string>> mPackets;
	};
}

// Game
namespace Game {
	void ModifierCheck::Run(Instance& game, Lua& lua, const std::filesystem::path& reportPath) {
		std::ofstream report(reportPath);
		lua.LoadBuffer(helpers);

		Checker checker(game, lua, report);
		checker.RunAll();
	}
}
