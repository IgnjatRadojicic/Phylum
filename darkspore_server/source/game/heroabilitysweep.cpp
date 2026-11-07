
// Include
#include "heroabilitysweep.h"

#include "instance.h"
#include "lua.h"
#include "modifiersystem.h"
#include "noun.h"
#include "object.h"
#include "objectmanager.h"
#include "passivemodifiers.h"

#include "sporenet/user.h"
#include "utils/functions.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <thread>

// Game
namespace Game {
	namespace {
		constexpr const char* classFolder = "data/serverdata/playerclass";
		constexpr const char* mobNounName = "ZelemBasicHybrid.Noun";
		constexpr float durable = 100000;
		constexpr float enemyDistance = 6;
		// A hurt friend behind the hero, so heals and buffs on allies show.
		constexpr float allyDistance = 4;
		constexpr float spotSpacing = 200;
		constexpr int watchMilliseconds = 3000;
		constexpr int stepMilliseconds = 20;

		// Records the missing engine functions a script asks for, as the script check does.
		constexpr const char* collectGaps = R"lua(
			HeroSweepGaps = {}
			local record = ScriptGaps.Record
			ScriptGaps.Record = function(name)
				HeroSweepGaps[#HeroSweepGaps + 1] = name
				record(name)
			end
			end
		)lua";

		struct Slot {
			PlayerAbility ability;
			const char* label;
		};

		constexpr Slot slots[] = {
			{ PlayerAbility::Special1, "basic" },
			{ PlayerAbility::Special2, "special1" },
			{ PlayerAbility::Special3, "special2" },
			{ PlayerAbility::Passive, "squad" },
		};

		// The hero classes a player can have: no older versions (_v1...) and no test classes.
		std::vector<std::string> HeroClassNames() {
			std::vector<std::string> names;
			if (!std::filesystem::exists(classFolder)) {
				return names;
			}
			for (const auto& entry : std::filesystem::directory_iterator(classFolder)) {
				auto name = entry.path().filename().string();
				const auto suffix = std::string(".playerClass.xml");
				if (!name.ends_with(suffix)) {
					continue;
				}
				name.resize(name.size() - suffix.size());
				const bool version = name.find("_v") != std::string::npos;
				const bool test = name.find("Test") != std::string::npos || name.find("test") != std::string::npos;
				if (!version && !test) {
					names.push_back(name);
				}
			}
			std::sort(names.begin(), names.end());
			return names;
		}

		class Sweep {
			public:
				Sweep(Instance& game, Lua& lua, std::ostream& report, const std::string& classFilter) :
					mGame(game), mLua(lua), mReport(report), mClassFilter(classFilter), mPackets(std::make_shared<std::vector<std::string>>()) {
					const auto packets = mPackets;
					mGame.StartOffline(std::make_shared<SporeNet::User>("hero-sweep"), [packets](std::string_view packet) {
						packets->emplace_back(packet);
					});
					mLua.SetTraceObserver([this](const std::string& line) {
						if (line.starts_with("ERROR")) {
							mErrors.push_back(line);
						}
					});
					mLua.LoadBuffer(collectGaps);
				}

				~Sweep() {
					mLua.SetTraceObserver(nullptr);
				}

				void RunAll() {
					size_t worked = 0, total = 0;
					for (const auto& className : HeroClassNames()) {
						if (!mClassFilter.empty() && className.find(mClassFilter) == std::string::npos) {
							continue;
						}
						const auto noun = NounDatabase::Instance().Get(utils::hash_id(className + ".Noun"));
						const auto playerClass = noun ? noun->GetPlayerClassData() : nullptr;
						if (!playerClass) {
							mReport << "CLASS " << className << " no player class data\n";
							continue;
						}

						mReport << "CLASS " << className << "\n";
						for (const auto& slot : slots) {
							const auto abilityId = playerClass->GetAbility(slot.ability);
							if (abilityId == 0) {
								continue;
							}
							++total;
							worked += Cast(className, slot.label, abilityId);
						}
						mReport.flush();
					}
					mReport << "Hero abilities that did something with no error: " << worked << " of " << total << "\n";
				}

			private:
				void Step(int milliseconds) {
					for (int waited = 0; waited < milliseconds; waited += stepMilliseconds) {
						std::this_thread::sleep_for(std::chrono::milliseconds(stepMilliseconds));
						mGame.Update();
					}
				}

				ObjectPtr Create(uint32_t noun, uint8_t team, const glm::vec3& position) {
					auto object = mGame.GetObjectManager().Create(noun);
					if (object) {
						object->SetAttributeValue(AttributeType::MaxHealth, durable);
						object->SetAttributeValue(AttributeType::MaxMana, durable);
						object->SetHealth(durable);
						object->SetMana(durable);
						object->SetTeam(team);
						object->SetPosition(position);
					}
					return object;
				}

				// radius 40 is what a cast counts as its own; cleanup takes half the spacing to the next spot,
				// so pets and shots that strayed cannot reach the next cast.
				std::set<uint32_t> ObjectsNear(const glm::vec3& position, float radius = 40.f) const {
					std::set<uint32_t> ids;
					for (const auto& object : mGame.GetObjectManager().GetObjectsInRadius(BoundingSphere(position, radius), {})) {
						if (!object->IsMarkedForDeletion()) {
							ids.insert(object->GetId());
						}
					}
					return ids;
				}

				// Each cast gets its own spot far from the last, so leftovers cannot touch it.
				bool Cast(const std::string& className, const char* slot, uint32_t abilityId) {
					const auto ability = mLua.GetAbility(abilityId);
					// A grid inside the world's octree (+-2500), where object searches find things.
					const auto index = mCasts++;
					const glm::vec3 spot(-2000.f + spotSpacing * static_cast<float>(index % 20), -2000.f + spotSpacing * static_cast<float>(index / 20), 0.f);
					mReport << "  " << slot << " " << (ability ? ability->GetName() : std::to_string(abilityId));
					if (!ability) {
						mReport << " NO-SCRIPT\n";
						return false;
					}

					auto hero = Create(utils::hash_id(className + ".Noun"), 1, spot);
					auto enemy = Create(utils::hash_id(mobNounName), 2, spot + glm::vec3(enemyDistance, 0.f, 0.f));
					auto ally = Create(utils::hash_id(mobNounName), 1, spot - glm::vec3(allyDistance, 0.f, 0.f));
					if (!hero || !enemy || !ally) {
						mReport << " NO-OBJECTS\n";
						return false;
					}
					// As in a game, where it starts once the client knows the hero (Soul Ravager's souls).
					PassiveModifiers(mLua).Start(hero);

					Step(100);
					// Half health, so a heal has room to show.
					hero->SetHealth(durable / 2);
					ally->SetHealth(durable / 2);
					const auto before = ObjectsNear(spot);
					const float enemyHealth = enemy->GetHealth();
					const float heroHealth = hero->GetHealth();
					const float allyHealth = ally->GetHealth();
					mPackets->clear();
					mErrors.clear();
					sol::state_view(mLua.GetState())["HeroSweepGaps"] = sol::new_table();

					RakNet::CombatData cast {};
					cast.abilityId = abilityId;
					cast.abilityRank = 1;
					cast.targetId = enemy->GetId();
					cast.cursorPosition = enemy->GetPosition();
					cast.targetPosition = enemy->GetPosition();
					mGame.UseAbility(hero, cast);
					Step(watchMilliseconds);

					// What it did.
					const auto after = ObjectsNear(spot);
					size_t spawned = 0;
					for (const auto id : after) {
						spawned += !before.contains(id);
					}
					const float damage = enemyHealth - enemy->GetHealth();
					const float healed = hero->GetHealth() - heroHealth;
					const float allyHealed = ally->GetHealth() - allyHealth;
					const auto& modifiers = mGame.GetModifiers().GetManager();
					const auto onHero = modifiers.GetAll(hero->GetId()).size();
					const auto onEnemy = modifiers.GetAll(enemy->GetId()).size();
					const auto onAlly = modifiers.GetAll(ally->GetId()).size();
					const bool running = hero->GetLuaThread() != nullptr;
					const auto moved = glm::distance(hero->GetPosition(), spot);

					std::map<std::string, size_t> packets;
					for (const auto& packet : *mPackets) {
						++packets[packet];
					}

					std::set<std::string> gaps;
					sol::table gapTable = sol::state_view(mLua.GetState())["HeroSweepGaps"];
					for (const auto& [_, name] : gapTable) {
						gaps.insert(name.as<std::string>());
					}

					const bool acted = damage > 0 || healed > 0 || allyHealed > 0 || onHero > 0 || onEnemy > 0 || onAlly > 0 || spawned > 0 || moved > 0.5f ||
						packets.contains("ObjectJump") || packets.contains("ObjectTeleport");
					const bool good = acted && mErrors.empty() && gaps.empty();

					mReport << (good ? " OK" : (!mErrors.empty() ? " ERROR" : (!gaps.empty() ? " MISSING" : (acted ? " OK" : " NOTHING"))))
						<< std::format(" damage={:.0f} healed={:.0f} allyHealed={:.0f} heroModifiers={} enemyModifiers={} allyModifiers={} spawned={} moved={:.1f} running={} animations={}",
							damage, healed, allyHealed, onHero, onEnemy, onAlly, spawned, moved, running, packets["SetAnimationState"]);
					for (const auto& gap : gaps) {
						mReport << " missing:" << gap;
					}
					mReport << "\n";
					for (const auto& error : mErrors) {
						auto line = error.substr(0, error.find('\n'));
						mReport << "    " << line << "\n";
					}

					// Clean up, so the next cast starts from nothing.
					if (auto thread = hero->GetLuaThread()) {
						mLua.CancelThread(thread);
					}
					for (const auto id : ObjectsNear(spot, spotSpacing / 2)) {
						if (auto object = mGame.GetObjectManager().Get(id)) {
							object->MarkForDeletion();
						}
					}
					Step(100);
					return good;
				}

				Instance& mGame;
				Lua& mLua;
				std::ostream& mReport;
				std::string mClassFilter;
				std::shared_ptr<std::vector<std::string>> mPackets;
				std::vector<std::string> mErrors;
				uint32_t mCasts = 0;
		};
	}

	// HeroAbilitySweep
	void HeroAbilitySweep::Run(Instance& game, Lua& lua, const std::filesystem::path& reportPath, const std::string& classFilter) {
		std::ofstream report(reportPath);
		Sweep(game, lua, report, classFilter).RunAll();
	}
}
