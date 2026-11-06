
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

		-- A modifier for the checks: its deactivate does what real ones do.
		ModifierCheckHelper = { objects = {}, stops = 0 }
		nModifier.RegisterModifier("ModifierCheckHelper", {
			activationType = nActivationType.Unique,
			deactivate = function()
				local helper = ModifierCheckHelper
				helper.stops = helper.stops + 1
				for _, id in ipairs(helper.objects) do
					nGameObject.MarkForDelete(id)
				end
				helper.objects = {}
				if helper.requestAgain and helper.stops < 5 then
					nModifier.RequestModifier(helper.requestAgain, helper.requestAgain, nUtil.SPID("ModifierCheckHelper"))
				end
			end
		})

		-- Runs after the helper's deactivate has marked its object, and looks back at the first object.
		nModifier.RegisterModifier("ModifierCheckWitness", {
			activationType = nActivationType.Unique,
			deactivate = function()
				ModifierCheckHelper.sawFirst = nObjectManager.IsValidObject(ModifierCheckHelper.first)
			end
		})

		function ModifierCheckWitnessGuid()
			return nUtil.SPID("ModifierCheckWitness")
		end

		-- Reads a property of its own request, as modifier_pushpullboss does.
		nModifier.RegisterModifier("ModifierCheckProperty", {
			CanUseModifier = function()
				ModifierCheckHelper.sawProperty = nModifier.GetFloatProperty(nil, 0)
				return true
			end
		})

		function ModifierCheckPropertyRequest(agent, result)
			nThread.Create(agent, function()
				local caller = { instanceId = 0 }
				CastContext.Set(caller)
				nModifier.RequestModifier(agent, agent, nUtil.SPID("ModifierCheckProperty"), nil, 1, 7)
				result.restored = CastContext.Get() == caller
			end)
		end

		-- Not in a thread, where coroutine.running gives nil.
		function ModifierCheckPropertyRequestOnMain(agent)
			nModifier.RequestModifier(agent, agent, nUtil.SPID("ModifierCheckProperty"), nil, 1, 8)
		end

		-- Abilities for the checks, set up the way scripts with no template do it.
		ModifierCheckCast = { casts = 0, stopped = 0 }
		local waits = {}
		nAbility.RegisterAbility("ModifierCheckWaits", waits)
		waits[nAbilityFns.Tick] = function()
			ModifierCheckCast.casts = ModifierCheckCast.casts + 1
			nThread.WaitForXSeconds(0.05)
		end

		local fails = {}
		nAbility.RegisterAbility("ModifierCheckFails", fails)
		fails[nAbilityFns.Tick] = function()
			error("this ability fails on purpose")
		end
		fails[nAbilityFns.Deactivate] = function()
			ModifierCheckCast.stopped = nAbility.GetAgentID()
		end

		-- A modifier whose script dies at start. It has no duration, so only the failure can end it.
		ModifierCheckBroken = { stops = 0 }
		nModifier.RegisterModifier("ModifierCheckBroken", {
			activationType = nActivationType.Unique,
			activate = function()
				error("this modifier fails on purpose")
			end,
			deactivate = function()
				ModifierCheckBroken.stops = ModifierCheckBroken.stops + 1
			end
		})

		-- Two threads one after the other: the second usually gets the first one's pooled
		-- Lua thread, and must not find the first one's cast.
		function ModifierCheckStaleCast(object, result)
			nThread.Create(object, function()
				result.first = coroutine.running()
				CastContext.Set({ stale = true })
			end)
			nThread.Create(object, function()
				result.reused = coroutine.running() == result.first
				result.stale = CastContext.Get() ~= nil
			end)
		end

		function ModifierCheckBrokenGuid()
			return nUtil.SPID("ModifierCheckBroken")
		end

		function ModifierCheckHelperGuid()
			return nUtil.SPID("ModifierCheckHelper")
		end

		function ModifierCheckForgetHelper()
			nModifier.registered[nUtil.SPID("ModifierCheckHelper")] = nil
			nModifier.registered[nUtil.SPID("ModifierCheckWitness")] = nil
			nModifier.registered[nUtil.SPID("ModifierCheckProperty")] = nil
			nModifier.registered[nUtil.SPID("ModifierCheckBroken")] = nil
			nAbility.scripts.ModifierCheckWaits = nil
			nAbility.scripts.ModifierCheckFails = nil
		end

		-- An ability has an instance number of its own, which can equal a modifier's id.
		function ModifierCheckAbilityChange(agent, abilityInstance)
			nThread.Create(agent, function()
				CastContext.Set({ instanceId = abilityInstance })
				nAttribute.AddAttributeModifier(agent, nAttributeType.Strength, 5)
			end)
		end

		function ModifierCheckMarkNothing(agent, running)
			nThread.Create(agent, function()
				CastContext.Set(ModifierEngine.GetContext(running))
				nModifier.MarkForDelete(agent, nModifier.GetFirstModifierByGUID(agent, 1))
			end)
		end

		function ModifierCheckChangeInside(running, object)
			nThread.Create(object, function()
				CastContext.Set(ModifierEngine.GetContext(running))
				nAttribute.AddAttributeModifier(object, nAttributeType.Strength, 5)
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
				CheckAbilityChangeOutlivesModifier(slow);
				CheckMissingIdMarksNothing(slow);
				CheckFullyResistedIsRefused(slow);
				CheckReusedIdKeepsItsChanges(slow);
				CheckResetAnimationReachesTheClient();
				CheckPrimaryAttributeCanBeRead();
				CheckHashedModulesLoad();
				CheckSecondCastRuns();
				CheckDistanceCanBeRead();
				CheckMeleeArcFindsTargets();
				CheckFailedCastRunsDeactivate();
				CheckDeactivateCanDeleteObjects(slow);
				CheckDeactivateCanRequestItself();
				CheckMarkedObjectsOutlastDeactivate();
				CheckCanUseReadsItsOwnContext();
				CheckFailedStartEndsIt();
				CheckReusedThreadForgetsItsCast();

				sol::protected_function forget = mState["ModifierCheckForgetHelper"];
				forget();
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

			void CheckDeactivateCanDeleteObjects(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();

				sol::protected_function helperGuid = mState["ModifierCheckHelperGuid"];
				const uint32_t guid = helperGuid();

				// Enough that adding them moves the list being walked to a new block of memory.
				std::vector<uint32_t> ids;
				sol::table objects = mState["ModifierCheckHelper"]["objects"];
				for (int index = 1; index <= 64; ++index) {
					const auto object = Spawn();
					objects[index] = object->GetId();
					ids.push_back(object->GetId());
				}

				// An object deleted during cleanup needs cleaning up too.
				const auto carrierId = ids.front();
				Request(mGame.GetObjectManager().Get(carrierId), caster, slow);

				const auto agentId = agent->GetId();
				const auto id = Request(agent, caster, guid);
				agent->MarkForDeletion();
				agent.reset();
				mGame.GetObjectManager().Update(0.05f);

				const auto left = std::count_if(ids.begin(), ids.end(), [this](uint32_t object) {
					return mGame.GetObjectManager().Get(object) != nullptr;
				});
				const auto leaked = Manager().GetAll(carrierId).size();
				Report("deactivate-can-delete-objects", id != 0 && left == 0 && leaked == 0 && Manager().GetAll(agentId).empty(),
					"left=" + std::to_string(left) + " leaked=" + std::to_string(leaked));
			}

			void EndNow(uint32_t id) {
				mGame.GetModifiers().MarkForDelete(id);
				mGame.GetModifiers().Update();
			}

			float Strength(const Game::ObjectPtr& object) {
				return object->GetAttributeValue(Game::AttributeType::Strength);
			}

			void CheckAbilityChangeOutlivesModifier(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();
				const float before = Strength(agent);

				const auto id = Request(agent, caster, slow);
				sol::protected_function change = mState["ModifierCheckAbilityChange"];
				change(agent->GetId(), id);
				EndNow(id);

				Report("ability-change-outlives-modifier", id != 0 && Strength(agent) == before + 5,
					"before=" + std::to_string(before) + " after=" + std::to_string(Strength(agent)));
			}

			void CheckMissingIdMarksNothing(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();

				const auto id = Request(agent, caster, slow);
				sol::protected_function mark = mState["ModifierCheckMarkNothing"];
				mark(agent->GetId(), id);
				mGame.GetModifiers().Update();

				Report("missing-id-marks-nothing", id != 0 && Manager().Get(id), "");
				EndNow(id);
			}

			void CheckFullyResistedIsRefused(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();
				agent->SetAttributeValue(Game::AttributeType::DebuffDuration, 1);
				agent->SetAttributeValue(Game::AttributeType::CrowdControlDurationDecrease, 1);

				const auto id = Request(agent, caster, slow);
				const auto running = Manager().GetAll(agent->GetId()).size();

				Report("fully-resisted-is-refused", id == 0 && running == 0, "running=" + std::to_string(running));
				for (uint32_t left : Manager().GetAll(agent->GetId())) {
					EndNow(left);
				}
			}

			void CheckReusedThreadForgetsItsCast() {
				auto object = Spawn();
				sol::table result = mState.create_table();
				sol::protected_function check = mState["ModifierCheckStaleCast"];
				check(object->GetId(), result);

				const bool reused = result.get_or("reused", false);
				const bool stale = result.get_or("stale", true);
				Report("reused-thread-forgets-its-cast", reused && !stale,
					"reused=" + std::to_string(reused) + " stale=" + std::to_string(stale));
			}

			void CheckFailedStartEndsIt() {
				auto agent = Spawn();
				auto caster = Spawn();

				sol::protected_function brokenGuid = mState["ModifierCheckBrokenGuid"];
				const uint32_t guid = brokenGuid();

				const auto id = Request(agent, caster, guid);
				const bool startedRunning = id != 0 && static_cast<bool>(Manager().Get(id));
				Wait(100);

				const int stops = mState["ModifierCheckBroken"]["stops"];
				Report("failed-start-ends-it", startedRunning && !Manager().Get(id) && stops == 1,
					"id=" + std::to_string(id) + " left=" + std::to_string(static_cast<bool>(Manager().Get(id))) + " stops=" + std::to_string(stops));
			}

			void CheckDeactivateCanRequestItself() {
				auto agent = Spawn();
				auto caster = Spawn();

				sol::protected_function helperGuid = mState["ModifierCheckHelperGuid"];
				const uint32_t guid = helperGuid();

				sol::table helper = mState["ModifierCheckHelper"];
				helper["stops"] = 0;

				const auto agentId = agent->GetId();
				const auto id = Request(agent, caster, guid);
				helper["requestAgain"] = agentId;

				agent->MarkForDeletion();
				agent.reset();
				mGame.GetObjectManager().Update(0.05f);
				helper["requestAgain"] = sol::lua_nil;

				const int stops = helper["stops"];
				Report("deactivate-can-request-itself", id != 0 && stops == 1 && Manager().GetAll(agentId).empty(),
					"stops=" + std::to_string(stops) + " left=" + std::to_string(Manager().GetAll(agentId).size()));
			}

			void CheckReusedIdKeepsItsChanges(uint32_t slow) {
				auto agent = Spawn();
				auto caster = Spawn();
				auto other = Spawn();

				const auto otherId = other->GetId();
				const auto id = Request(agent, caster, slow);
				sol::protected_function change = mState["ModifierCheckChangeInside"];
				change(id, otherId);

				other->MarkForDeletion();
				other.reset();
				mGame.GetObjectManager().Update(0.05f);

				auto reused = Spawn();
				const float before = Strength(reused);
				reused->AddAttributeChange(static_cast<uint8_t>(Game::AttributeType::Strength), 5, Game::AttributeOp::Add);
				EndNow(id);

				Report("reused-id-keeps-its-changes", reused->GetId() == otherId && Strength(reused) == before + 5,
					"reused=" + std::to_string(reused->GetId() == otherId) + " before=" + std::to_string(before) + " after=" + std::to_string(Strength(reused)));
			}

			void CheckMarkedObjectsOutlastDeactivate() {
				auto first = Spawn();
				auto second = Spawn();

				sol::protected_function helperGuid = mState["ModifierCheckHelperGuid"];
				sol::protected_function witnessGuid = mState["ModifierCheckWitnessGuid"];
				const uint32_t helperId = helperGuid();
				const uint32_t witnessId = witnessGuid();

				sol::table helper = mState["ModifierCheckHelper"];
				sol::table objects = helper["objects"];
				objects[1] = second->GetId();
				helper["first"] = first->GetId();
				helper["sawFirst"] = sol::lua_nil;

				Request(first, first, helperId);
				Request(second, second, witnessId);

				// Only the first object is marked here. Its deactivate marks the second.
				first->MarkForDeletion();
				first.reset();
				second.reset();
				mGame.GetObjectManager().Update(0.05f);

				const bool sawFirst = helper.get_or("sawFirst", false);
				Report("marked-objects-outlast-deactivate", sawFirst, "sawFirst=" + std::to_string(sawFirst));
			}

			void CheckCanUseReadsItsOwnContext() {
				auto agent = Spawn();

				sol::table helper = mState["ModifierCheckHelper"];
				sol::table result = mState.create_table();

				sol::protected_function request = mState["ModifierCheckPropertyRequest"];
				request(agent->GetId(), result);
				const double inThread = helper.get_or("sawProperty", 0.0);
				const bool restored = result.get_or("restored", false);

				sol::protected_function requestOnMain = mState["ModifierCheckPropertyRequestOnMain"];
				sol::protected_function_result onMain = requestOnMain(agent->GetId());
				const double mainValue = helper.get_or("sawProperty", 0.0);

				Report("can-use-reads-its-own-context", inThread == 7 && restored && onMain.valid() && mainValue == 8,
					"thread=" + std::to_string(inThread) + " restored=" + std::to_string(restored) + " main=" + std::to_string(mainValue));

				for (uint32_t running : Manager().GetAll(agent->GetId())) {
					EndNow(running);
				}
			}

			void CheckResetAnimationReachesTheClient() {
				auto agent = Spawn();

				mPackets->clear();
				sol::protected_function reset = mState["nGameObject"]["ResetAnimationState"];
				reset(agent->GetId());

				Report("reset-animation-reaches-the-client", Saw("SetAnimationState"), "");
			}

			void CheckPrimaryAttributeCanBeRead() {
				auto agent = Spawn();

				sol::protected_function primary = mState["nAttribute"]["GetPrimaryAttributeValue"];
				sol::protected_function_result result = primary(agent->GetId());

				Report("primary-attribute-can-be-read", result.valid(), "");
			}

			// This ability requires a module that only exists under the hash of its name.
			void CheckHashedModulesLoad() {
				sol::object ability = mState["nAbility"]["scripts"]["LightningRogueActive"];
				Report("hashed-modules-load", ability.is<sol::table>(), "");
			}

			void CheckSecondCastRuns() {
				auto agent = Spawn();
				const auto ability = mLua.GetAbility(utils::hash_id("ModifierCheckWaits"));
				ability->FindNumberedTick();

				ability->Tick(agent, nullptr, glm::vec3(0.f), 1);
				ability->Tick(agent, nullptr, glm::vec3(0.f), 1);
				Wait(120);

				sol::table cast = mState["ModifierCheckCast"];
				const int casts = cast["casts"];
				Report("second-cast-runs", casts == 2, "casts=" + std::to_string(casts));
			}

			// Scripts give a position with no height, then its radius. This facing is along X.
			void CheckMeleeArcFindsTargets() {
				sol::protected_function arc = mState["nMathUtil"]["CircleIntersectsArc"];
				constexpr float quarterTurn = 1.5707964f;

				sol::protected_function_result ahead = arc(1.f, 0.f, 0.5f, 0.f, 0.f, 1.f, 0.f, 2.f, quarterTurn);
				sol::protected_function_result behind = arc(-1.f, 0.f, 0.5f, 0.f, 0.f, 1.f, 0.f, 2.f, quarterTurn);

				const bool hitsAhead = ahead.valid() && ahead.get<bool>();
				const bool hitsBehind = !behind.valid() || behind.get<bool>();
				Report("melee-arc-finds-targets", hitsAhead && !hitsBehind,
					"ahead=" + std::to_string(hitsAhead) + " behind=" + std::to_string(hitsBehind));
			}

			void CheckDistanceCanBeRead() {
				auto first = Spawn();
				auto second = Spawn();
				first->SetPosition(glm::vec3(0.f));
				second->SetPosition(glm::vec3(3.f, 4.f, 0.f));

				sol::protected_function distance = mState["nGameObject"]["GetObjectDistance"];
				sol::protected_function_result result = distance(first->GetId(), second->GetId());
				sol::optional<float> value = result;

				Report("distance-can-be-read", value && *value == 5.f, "distance=" + std::to_string(value.value_or(-1.f)));
			}

			void CheckFailedCastRunsDeactivate() {
				auto agent = Spawn();
				const auto ability = mLua.GetAbility(utils::hash_id("ModifierCheckFails"));
				ability->FindNumberedTick();

				ability->Tick(agent, nullptr, glm::vec3(0.f), 1);

				sol::table cast = mState["ModifierCheckCast"];
				const uint32_t stopped = cast["stopped"];
				Report("failed-cast-runs-deactivate", stopped == agent->GetId(), "stopped=" + std::to_string(stopped));
			}

			// Not a pass or a fail: this finds which scripts need engine functions we do not have yet.
			void RunEveryScript() {
				sol::protected_function all = mState["ModifierCheckAll"];
				sol::table guids = all();

				size_t started = 0;
				std::vector<uint32_t> refused;
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
						refused.push_back(guid);
					}

					auto& objects = mGame.GetObjectManager();
					agent->MarkForDeletion();
					caster->MarkForDeletion();
					objects.Update(0.05f);
				}

				mReport << "SCRIPTS started=" << started << " refused=" << refused.size() << " failed=" << failures.size() << "\n";
				for (uint32_t guid : refused) {
					mReport << "REFUSED " << guid << "\n";
				}
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
