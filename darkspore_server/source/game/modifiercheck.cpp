
// Include
#include "modifiercheck.h"

#include "actionevents.h"
#include "damageevents.h"
#include "instance.h"
#include "lua.h"
#include "modifiersystem.h"
#include "object.h"
#include "objectmanager.h"
#include "locomotion.h"
#include "passivemodifiers.h"
#include "scripttriggervolumes.h"

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

		-- A cast that stealths its own caster, as ShadowRavagerSupport does before its first wait.
		local stealthCast = {}
		nAbility.RegisterAbility("ModifierCheckStealthCast", stealthCast)
		stealthCast[nAbilityFns.Tick] = function()
			local agent = nAbility.GetAgentID()
			nModifier.RequestModifier(agent, agent, nUtil.SPID("StealthModifier"))
			nThread.WaitForXSeconds(100)
		end

		-- A cast that raises a stat for as long as it runs, as charges and channels do.
		ModifierCheckCancel = {}
		local cancelled = {}
		nAbility.RegisterAbility("ModifierCheckCancelled", cancelled)
		cancelled[nAbilityFns.Activate] = function()
			nThreadData.GetPrivateTable().boost = nAttribute.AddAttributeModifier(nAbility.GetAgentID(), nAttributeType.Strength, 5)
		end
		cancelled[nAbilityFns.Tick] = function()
			nThread.WaitForXSeconds(100)
		end
		cancelled[nAbilityFns.Deactivate] = function()
			nAttribute.RemoveAttributeModifier(nAbility.GetAgentID(), nThreadData.GetPrivateTable().boost)
			ModifierCheckCancel.ended = true
		end

		-- The same, starting a thread that ends at once before its first wait, as the missile
		-- tempest starts its launches.
		local withChild = {}
		nAbility.RegisterAbility("ModifierCheckCancelledWithChild", withChild)
		withChild[nAbilityFns.Activate] = cancelled[nAbilityFns.Activate]
		withChild[nAbilityFns.Deactivate] = cancelled[nAbilityFns.Deactivate]
		withChild[nAbilityFns.Tick] = function()
			nThread.Create(nAbility.GetAgentID(), function() end)
			nThread.WaitForXSeconds(100)
		end

		-- A cast that leaves its change to the engine and whose Deactivate dies.
		local deactivateFails = {}
		nAbility.RegisterAbility("ModifierCheckDeactivateFails", deactivateFails)
		deactivateFails[nAbilityFns.Activate] = function()
			nAttribute.AddAttributeModifier(nAbility.GetAgentID(), nAttributeType.Strength, 5)
		end
		deactivateFails[nAbilityFns.Tick] = cancelled[nAbilityFns.Tick]
		deactivateFails[nAbilityFns.Deactivate] = function()
			error("this deactivate fails on purpose")
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

		-- An ability that ends at once, so its thread goes straight back to the pool.
		local finishes = {}
		nAbility.RegisterAbility("ModifierCheckFinishes", finishes)
		finishes[nAbilityFns.Tick] = function()
			ModifierCheckCast.finishedOn = coroutine.running()
		end

		-- Runs after that cast, usually on the same pooled thread, which must have the plain globals again.
		function ModifierCheckThreadGlobals(object, result)
			nThread.Create(object, function()
				result.reused = coroutine.running() == ModifierCheckCast.finishedOn
				result.plain = getfenv(0) == _G
			end)
		end

		-- Starts a thread that runs forever, as modifier_cryosboss_passive's spawner does.
		ModifierCheckParent = { ticks = 0 }
		nModifier.RegisterModifier("ModifierCheckParent", {
			activationType = nActivationType.Unique,
			activate = function()
				nThread.CreateThreadForObject(nAbility.GetAgentID(), function()
					while true do
						ModifierCheckParent.ticks = ModifierCheckParent.ticks + 1
						nThread.WaitForXSeconds(0.02)
					end
				end)
			end
		})

		function ModifierCheckParentGuid()
			return nUtil.SPID("ModifierCheckParent")
		end

		-- Writes down every damage event it is given.
		ModifierCheckListener = { heard = {} }
		nModifier.RegisterModifier("ModifierCheckListener", {
			activationType = nActivationType.Unique,
			handledEvents = nBit.Or(nAbilityEventFlags.TookDamage, nAbilityEventFlags.DealtDamage),
			[nAbilityFns.HandleEvent] = function(event)
				local heard = ModifierCheckListener.heard
				heard[#heard + 1] = {
					type = nAbility.GetAbilityEventType(event),
					other = nAbility.GetAbilityEventGUIDData(event, 1),
					amount = nAbility.GetAbilityEventFloatData(event, 2)
				}
				return true
			end
		})

		function ModifierCheckListenerGuid()
			return nUtil.SPID("ModifierCheckListener")
		end

		function ModifierCheckHeard(index, field)
			local entry = ModifierCheckListener.heard[index]
			return entry and entry[field] or 0
		end

		-- A cast and a modifier that hear InterruptModifier and answer what ModifierCheckEvents.answer
		-- holds: nil keeps them running, false ends them. order shows who heard it first.
		ModifierCheckEvents = { order = "", castEnded = 0, modifierEnded = 0 }
		local interrupt = nAbilityEventFlags.InterruptModifier
		local listens = { handledEvents = interrupt }
		nAbility.RegisterAbility("ModifierCheckListensCast", listens)
		listens[nAbilityFns.Tick] = function()
			nThread.WaitForXSeconds(100)
		end
		listens[nAbilityFns.HandleEvent] = function(event)
			ModifierCheckEvents.order = ModifierCheckEvents.order .. "c"
			return ModifierCheckEvents.answer
		end
		listens[nAbilityFns.Deactivate] = function()
			ModifierCheckEvents.castEnded = ModifierCheckEvents.castEnded + 1
		end

		nModifier.RegisterModifier("ModifierCheckListensModifier", {
			activationType = nActivationType.Unique,
			handledEvents = interrupt,
			[nAbilityFns.HandleEvent] = function(event)
				ModifierCheckEvents.order = ModifierCheckEvents.order .. "m"
				return ModifierCheckEvents.answer
			end,
			deactivate = function()
				ModifierCheckEvents.modifierEnded = ModifierCheckEvents.modifierEnded + 1
			end
		})

		function ModifierCheckListensModifierGuid()
			return nUtil.SPID("ModifierCheckListensModifier")
		end

		-- Ends on BreakRoot, as nModifier_Root_Template's handleEvent answers. The game's roots never
		-- do: each sets a numbered HandleEvent that calls handleEvent and drops its answer.
		nModifier.RegisterModifier("ModifierCheckBreaksOnTeleport", {
			activationType = nActivationType.Unique,
			handledEvents = nAbilityEventFlags.BreakRoot,
			[nAbilityFns.HandleEvent] = function(event)
				return nAbility.GetAbilityEventType(event) ~= nAbilityEventFlags.BreakRoot
			end
		})

		-- A channel set up as Sleeping Cloud's (nAbility_SleepingCloud): it needs its agent and ends
		-- on the agent's next action.
		ModifierCheckChannel = { ended = 0 }
		local channel = { requiresAgent = true, handledEvents = nAbilityEventFlags.StartedNewAction }
		nAbility.RegisterAbility("ModifierCheckChannel", channel)
		channel[nAbilityFns.Tick] = function()
			nThread.WaitForXSeconds(100)
		end
		channel[nAbilityFns.HandleEvent] = function(event)
			return nAbility.GetAbilityEventType(event) ~= nAbilityEventFlags.StartedNewAction
		end
		channel[nAbilityFns.Deactivate] = function()
			ModifierCheckChannel.ended = ModifierCheckChannel.ended + 1
		end

		-- Ends when its agent loses the modifier it watches, as Stealth Attack (nAbility_StealthAttack)
		-- ends when its stealth goes. watched is set by the check; heard counts the other GUIDs.
		ModifierCheckWatcher = { ended = 0, heard = 0 }
		local watcher = { handledEvents = nAbilityEventFlags.LostModifierOfType }
		nAbility.RegisterAbility("ModifierCheckWatcher", watcher)
		watcher[nAbilityFns.Tick] = function()
			nThread.WaitForXSeconds(100)
		end
		watcher[nAbilityFns.HandleEvent] = function(event)
			ModifierCheckWatcher.heard = ModifierCheckWatcher.heard + 1
			return nAbility.GetAbilityEventGUIDData(event, 1) ~= ModifierCheckWatcher.watched
		end
		watcher[nAbilityFns.Deactivate] = function()
			ModifierCheckWatcher.ended = ModifierCheckWatcher.ended + 1
		end

		function ModifierCheckBreaksOnTeleportGuid()
			return nUtil.SPID("ModifierCheckBreaksOnTeleport")
		end

		-- Sends the event to its own agent and ends itself on it, as a cast that breaks its own
		-- root would if it handled BreakRoot: it is running then, so it ends at its next wait.
		local selfSend = { handledEvents = interrupt }
		nAbility.RegisterAbility("ModifierCheckSendsToItself", selfSend)
		selfSend[nAbilityFns.Tick] = function()
			nAbility.SendAbilityEvent(nAbility.CreateAbilityEvent(interrupt), nAbility.GetAgentID())
			ModifierCheckEvents.afterSend = true
			nThread.WaitForXSeconds(100)
			ModifierCheckEvents.resumed = true
		end
		selfSend[nAbilityFns.HandleEvent] = function()
			return false
		end
		selfSend[nAbilityFns.Deactivate] = function()
			ModifierCheckEvents.selfEnded = true
		end

		-- Reads where it was cast only after a wait, as Quantum Blink does after its wind-up.
		local readsLater = {}
		nAbility.RegisterAbility("ModifierCheckReadsTargetLater", readsLater)
		readsLater[nAbilityFns.Tick] = function()
			nThread.WaitForXSeconds(0.05)
			ModifierCheckEvents.targetX, ModifierCheckEvents.targetY, ModifierCheckEvents.targetZ = nAbility.GetTargetPosition()
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
			nModifier.registered[nUtil.SPID("ModifierCheckParent")] = nil
			nModifier.registered[nUtil.SPID("ModifierCheckListener")] = nil
			nModifier.registered[nUtil.SPID("ModifierCheckListensModifier")] = nil
			nModifier.registered[nUtil.SPID("ModifierCheckBreaksOnTeleport")] = nil
			nAbility.scripts.ModifierCheckListensCast = nil
			nAbility.scripts.ModifierCheckSendsToItself = nil
			nAbility.scripts.ModifierCheckReadsTargetLater = nil
			nAbility.scripts.ModifierCheckChannel = nil
			nAbility.scripts.ModifierCheckWatcher = nil
			nAbility.scripts.ModifierCheckWaits = nil
			nAbility.scripts.ModifierCheckFails = nil
			nAbility.scripts.ModifierCheckCancelled = nil
			nAbility.scripts.ModifierCheckDeactivateFails = nil
			nAbility.scripts.ModifierCheckStealthCast = nil
			nAbility.scripts.ModifierCheckCancelledWithChild = nil
			nAbility.scripts.ModifierCheckFinishes = nil
			-- RegisterAbility also lists an ability as a modifier (one registry, as in the client)
			for _, name in ipairs({ "ModifierCheckWaits", "ModifierCheckFails", "ModifierCheckCancelled", "ModifierCheckDeactivateFails",
				"ModifierCheckStealthCast", "ModifierCheckCancelledWithChild", "ModifierCheckFinishes",
				"ModifierCheckListensCast", "ModifierCheckSendsToItself", "ModifierCheckChannel", "ModifierCheckWatcher",
				"ModifierCheckReadsTargetLater" }) do
				nModifier.registered[nUtil.SPID(name)] = nil
			end
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

		-- Only a running modifier's id runs the function, inside that modifier.
		function ModifierCheckCallInContext(agent, running, ended, result)
			nThread.Create(agent, function()
				local calls = 0
				local function count()
					calls = calls + 1
				end
				local before = CastContext.Get()
				nModifier.CallFunctionInContext(kObjIDNone, count)
				nModifier.CallFunctionInContext(nil, count)
				nModifier.CallFunctionInContext(ended, count)
				result.skipped = calls == 0
				result.agent = nModifier.CallFunctionInContext(running, nModifier.GetMyAgentID)
				result.restored = CastContext.Get() == before
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
	// Its AI definition names ZelemBasicMeleePassive, which needs no function the server lacks.
	constexpr const char* passiveNounName = "ZelemBasicMelee.Noun";
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
				CheckCallInContextNeedsARunningModifier(slow);
				CheckFullyResistedIsRefused(slow);
				CheckReusedIdKeepsItsChanges(slow);
				CheckResetAnimationReachesTheClient();
				CheckPrimaryAttributeCanBeRead();
				CheckHashedModulesLoad();
				CheckSecondCastRuns();
				CheckDistanceCanBeRead();
				CheckMeleeArcFindsTargets();
				CheckFailedCastRunsDeactivate();
				CheckFailedDeactivateUndoesItsChanges();
				CheckDeactivateCanDeleteObjects(slow);
				CheckDeactivateCanRequestItself();
				CheckMarkedObjectsOutlastDeactivate();
				CheckCanUseReadsItsOwnContext();
				CheckFailedStartEndsIt();
				CheckReusedThreadForgetsItsCast();
				CheckReusedThreadHasPlainGlobals();
				CheckChildThreadEndsWithIt();
				CheckDamageReachesHandlers();
				CheckEventAnswersEndOrKeep();
				CheckCastEndedFromItselfEndsAtItsWait();
				CheckCastKeepsItsTargetPoint();
				CheckTeleportBreaksRoot();
				CheckChannelEndsOnNewAction();
				CheckLostModifierEndsWatcher(slow);
				CheckSwapEndsFieldMedicBuff();
				CheckThornsHitsMeleeAttackerBack();
				CheckMobPassiveStartsOnce();
				CheckKillObjectEndsPassive();
				CheckDronePassiveSpawnsDrone();
				CheckAuraSlowsEnemyInside();
				CheckStealthEndsOnUsedAbility();
				CheckStealthCastKeepsItsStealth();
				CheckTimeBubbleEnds();
				CheckElectronSphereFlies();
				CheckCancelledCastUndoesItsChanges("ModifierCheckCancelled", "cancelled-cast-undoes-its-changes");
				CheckCancelledCastUndoesItsChanges("ModifierCheckCancelledWithChild", "cancelled-cast-with-child-undoes-its-changes");
				CheckJumpLandsWhereItAimed();
				CheckLineFindsMobOnIt();

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
				// The slow changes speed in its Tick, which starts on the next update (as in the client).
				mGame.GetLua().Update();
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

			void CheckCallInContextNeedsARunningModifier(uint32_t slow) {
				auto agent = Spawn();
				auto other = Spawn();
				auto caster = Spawn();

				const auto running = Request(agent, caster, slow);
				const auto ended = Request(other, caster, slow);
				EndNow(ended);

				sol::table result = mState.create_table();
				sol::protected_function call = mState["ModifierCheckCallInContext"];
				call(agent->GetId(), running, ended, result);

				const bool skipped = result.get_or("skipped", false);
				const bool restored = result.get_or("restored", false);
				const double seen = result.get_or("agent", 0.0);
				Report("call-in-context-needs-a-running-modifier",
					running != 0 && ended != 0 && skipped && restored && seen == agent->GetId(),
					"skipped=" + std::to_string(skipped) + " restored=" + std::to_string(restored) + " agent=" + std::to_string(seen));
				EndNow(running);
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

			// A cast gives its thread the ability's environment. The thread is renewed when it is
			// freed, so "reused" is expected to be false: the point is that the globals are plain.
			void CheckReusedThreadHasPlainGlobals() {
				auto agent = Spawn();
				const auto ability = mLua.GetAbility(utils::hash_id("ModifierCheckFinishes"));
				ability->FindNumberedTick();
				ability->Tick(agent, nullptr, glm::vec3(0.f), 1);

				sol::table result = mState.create_table();
				sol::protected_function check = mState["ModifierCheckThreadGlobals"];
				check(agent->GetId(), result);

				const bool reused = result.get_or("reused", false);
				const bool plain = result.get_or("plain", false);
				Report("reused-thread-has-plain-globals", plain, "reused=" + std::to_string(reused) + " plain=" + std::to_string(plain));
			}

			void CheckChildThreadEndsWithIt() {
				auto agent = Spawn();
				auto caster = Spawn();

				sol::protected_function parentGuid = mState["ModifierCheckParentGuid"];
				const uint32_t guid = parentGuid();
				const auto id = Request(agent, caster, guid);
				Wait(100);

				const int whileRunning = mState["ModifierCheckParent"]["ticks"];
				EndNow(id);
				const int atEnd = mState["ModifierCheckParent"]["ticks"];
				Wait(100);
				const int later = mState["ModifierCheckParent"]["ticks"];

				Report("child-thread-ends-with-it", id != 0 && whileRunning > 1 && later == atEnd,
					"whileRunning=" + std::to_string(whileRunning) + " atEnd=" + std::to_string(atEnd) + " later=" + std::to_string(later));
			}

			void CheckDamageReachesHandlers() {
				auto victim = Spawn();
				auto attacker = Spawn();

				sol::protected_function listenerGuid = mState["ModifierCheckListenerGuid"];
				const uint32_t guid = listenerGuid();
				Request(victim, victim, guid);
				Request(attacker, attacker, guid);

				Game::DamageReport hit;
				hit.victim = victim->GetId();
				hit.attacker = attacker->GetId();
				hit.amount = 25.f;
				mGame.ReportDamage(hit);

				sol::protected_function heard = mState["ModifierCheckHeard"];
				const uint32_t firstType = heard(1, "type");
				const uint32_t firstOther = heard(1, "other");
				const float firstAmount = heard(1, "amount");
				const uint32_t secondType = heard(2, "type");
				const uint32_t secondOther = heard(2, "other");

				const bool good = firstType == 1 && firstOther == attacker->GetId() && firstAmount == 25.f &&
					secondType == 8192 && secondOther == victim->GetId();
				Report("damage-reaches-handlers", good,
					"took=" + std::to_string(firstType) + "/" + std::to_string(firstOther) + "/" + std::to_string(firstAmount) +
					" dealt=" + std::to_string(secondType) + "/" + std::to_string(secondOther));
			}

			// An event reaches the agent's running cast first, then its modifiers (0x9deca0). A handler
			// that returns nothing keeps both running; an explicit false ends both.
			void CheckEventAnswersEndOrKeep() {
				auto agent = Spawn();
				const auto castId = utils::hash_id("ModifierCheckListensCast");
				mLua.GetAbility(castId)->FindNumberedTick();
				sol::protected_function modifierGuid = mState["ModifierCheckListensModifierGuid"];
				const auto modifierId = Request(agent, agent, modifierGuid());

				RakNet::CombatData cast {};
				cast.abilityId = castId;
				cast.abilityRank = 1;
				mGame.UseAbility(agent, cast);
				Wait(100);

				sol::table events = mState["ModifierCheckEvents"];
				const auto interrupt = Game::AbilityEvent(Game::AbilityEventType::InterruptModifier);
				const auto& modifiers = mGame.GetModifiers().GetManager();

				events["answer"] = sol::lua_nil;
				mGame.SendAbilityEvent(agent->GetId(), interrupt);
				Wait(100);
				const std::string keptOrder = events.get_or<std::string>("order", "");
				const bool castKept = agent->GetLuaThread() != nullptr && events.get_or("castEnded", 0) == 0;
				const bool modifierKept = modifiers.Get(modifierId) != nullptr && events.get_or("modifierEnded", 0) == 0;
				Report("event-nil-keeps-cast-and-modifier", modifierId != 0 && keptOrder == "cm" && castKept && modifierKept,
					"order=" + keptOrder + " cast=" + std::to_string(castKept) + " modifier=" + std::to_string(modifierKept));

				events["answer"] = false;
				mGame.SendAbilityEvent(agent->GetId(), interrupt);
				Wait(100);
				const std::string endedOrder = events.get_or<std::string>("order", "");
				const bool castEnded = agent->GetLuaThread() == nullptr && events.get_or("castEnded", 0) == 1;
				const bool modifierEnded = modifiers.Get(modifierId) == nullptr && events.get_or("modifierEnded", 0) == 1;
				Report("event-false-ends-cast-and-modifier", endedOrder == "cmcm" && castEnded && modifierEnded,
					"order=" + endedOrder + " cast=" + std::to_string(castEnded) + " modifier=" + std::to_string(modifierEnded));

				// Ended, so they hear no more.
				mGame.SendAbilityEvent(agent->GetId(), interrupt);
				const std::string afterOrder = events.get_or<std::string>("order", "");
				Report("ended-scripts-hear-no-more-events", afterOrder == "cmcm", "order=" + afterOrder);
			}

			// A cast that sends an event to its own agent and ends on it is running then, so it ends at
			// its next wait instead of going on (the client defers it, 0x9de9c0).
			void CheckCastEndedFromItselfEndsAtItsWait() {
				auto agent = Spawn();
				const auto castId = utils::hash_id("ModifierCheckSendsToItself");
				mLua.GetAbility(castId)->FindNumberedTick();

				RakNet::CombatData cast {};
				cast.abilityId = castId;
				cast.abilityRank = 1;
				mGame.UseAbility(agent, cast);
				Wait(200);

				sol::table events = mState["ModifierCheckEvents"];
				const bool sent = events.get_or("afterSend", false);
				const bool ended = events.get_or("selfEnded", false);
				const bool resumed = events.get_or("resumed", false);
				Report("cast-ended-from-itself-ends-at-its-wait", sent && ended && !resumed && agent->GetLuaThread() == nullptr,
					"sent=" + std::to_string(sent) + " ended=" + std::to_string(ended) + " resumed=" + std::to_string(resumed));
			}

			// A cast keeps the point it was cast at for its whole run, also after it first waits.
			void CheckCastKeepsItsTargetPoint() {
				auto agent = Spawn();
				const auto castId = utils::hash_id("ModifierCheckReadsTargetLater");
				mLua.GetAbility(castId)->FindNumberedTick();

				RakNet::CombatData cast {};
				cast.abilityId = castId;
				cast.abilityRank = 1;
				cast.cursorPosition = glm::vec3(12.5f, -7.25f, 3.f);
				mGame.UseAbility(agent, cast);
				Wait(200);

				sol::table events = mState["ModifierCheckEvents"];
				const float x = events.get_or("targetX", 0.f);
				const float y = events.get_or("targetY", 0.f);
				const float z = events.get_or("targetZ", 0.f);
				Report("cast-keeps-its-target-point", x == 12.5f && y == -7.25f && z == 3.f,
					std::format("read={},{},{}", x, y, z));
			}

			// A teleport raises BreakRoot before the object moves (0xa09b2c); TeleportAndFace raises
			// none. The game's EntangleModifier hears it but stays, as in the client: its numbered
			// HandleEvent drops the template's false.
			void CheckTeleportBreaksRoot() {
				auto agent = Spawn();
				sol::protected_function spid = mState["nUtil"]["SPID"];
				sol::protected_function breaksGuid = mState["ModifierCheckBreaksOnTeleportGuid"];
				const auto breaks = Request(agent, agent, breaksGuid());
				const auto entangle = Request(agent, agent, spid("EntangleModifier"));
				Wait(100);
				const auto& modifiers = mGame.GetModifiers().GetManager();
				const bool started = breaks != 0 && entangle != 0 && modifiers.Get(breaks) && modifiers.Get(entangle);

				const auto at = agent->GetPosition();
				sol::protected_function teleportAndFace = mState["nLocomotion"]["TeleportAndFace"];
				teleportAndFace(agent->GetId(), at.x + 1.f, at.y, at.z, 1.f, 0.f, 0.f);
				const bool keptByFace = modifiers.Get(breaks) != nullptr;

				sol::protected_function teleport = mState["nLocomotion"]["TeleportObject"];
				teleport(agent->GetId(), at.x + 2.f, at.y, at.z);
				const bool broken = modifiers.Get(breaks) == nullptr;
				const bool entangleStays = modifiers.Get(entangle) != nullptr;

				Report("teleport-breaks-root", started && keptByFace && broken && entangleStays,
					"started=" + std::to_string(started) + " keptByFace=" + std::to_string(keptByFace) +
					" broken=" + std::to_string(broken) + " entangleStays=" + std::to_string(entangleStays));
			}

			// A channel that ends on StartedNewAction, as Sleeping Cloud's does, survives its own start,
			// ends when its agent casts again (an ability that needs its agent) and when it is given
			// somewhere to go (MoveToObject, 0xa149c0).
			void CheckChannelEndsOnNewAction() {
				auto agent = Spawn();
				auto target = Spawn();
				const auto channelId = utils::hash_id("ModifierCheckChannel");
				mLua.GetAbility(channelId)->FindNumberedTick();
				sol::table channel = mState["ModifierCheckChannel"];

				RakNet::CombatData cast {};
				cast.abilityId = channelId;
				cast.abilityRank = 1;
				mGame.UseAbility(agent, cast);
				Wait(100);
				const bool survivesStart = channel.get_or("ended", 0) == 0 && agent->GetLuaThread() != nullptr;

				mGame.UseAbility(agent, cast);
				Wait(100);
				const bool endsOnCast = channel.get_or("ended", 0) == 1 && agent->GetLuaThread() != nullptr;

				sol::protected_function moveTo = mState["Locomotion"]["MoveToObject"];
				moveTo(agent->GetId(), target->GetId());
				Wait(100);
				const bool endsOnMove = channel.get_or("ended", 0) == 2 && agent->GetLuaThread() == nullptr;

				Report("channel-ends-on-new-action", survivesStart && endsOnCast && endsOnMove,
					"survivesStart=" + std::to_string(survivesStart) + " endsOnCast=" + std::to_string(endsOnCast) +
					" endsOnMove=" + std::to_string(endsOnMove) + " ended=" + std::to_string(channel.get_or("ended", 0)));
			}

			// When the agent's last modifier of a GUID ends, its casts and modifiers hear
			// LostModifierOfType with that GUID (0x9e1261). A watcher that answers false on its GUID ends.
			void CheckLostModifierEndsWatcher(uint32_t slow) {
				auto agent = Spawn();
				const auto watcherId = utils::hash_id("ModifierCheckWatcher");
				mLua.GetAbility(watcherId)->FindNumberedTick();
				sol::table watcher = mState["ModifierCheckWatcher"];
				watcher["watched"] = slow;

				const auto id = Request(agent, agent, slow);
				RakNet::CombatData cast {};
				cast.abilityId = watcherId;
				cast.abilityRank = 1;
				mGame.UseAbility(agent, cast);
				Wait(100);
				const bool running = id != 0 && agent->GetLuaThread() != nullptr && watcher.get_or("ended", 0) == 0;

				mGame.GetModifiers().MarkForDelete(id);
				Wait(100);
				const bool ended = agent->GetLuaThread() == nullptr && watcher.get_or("ended", 0) == 1;

				Report("lost-modifier-ends-watcher", running && ended && watcher.get_or("heard", 0) >= 1,
					"id=" + std::to_string(id) + " running=" + std::to_string(running) + " ended=" + std::to_string(ended) +
					" heard=" + std::to_string(watcher.get_or("heard", 0)));
			}

			// The game's FieldMedicHealthBuff ends when its hero leaves the field (PlayerSwap).
			void CheckSwapEndsFieldMedicBuff() {
				auto agent = Spawn();
				sol::protected_function spid = mState["nUtil"]["SPID"];
				const auto id = Request(agent, agent, spid("FieldMedicHealthBuff"));
				Wait(100);
				const auto& modifiers = mGame.GetModifiers().GetManager();
				const bool started = id != 0 && modifiers.Get(id) != nullptr;

				if (auto events = mGame.GetActionEvents()) {
					events->PlayerSwap(agent->GetId());
				}
				Wait(100);
				const bool ended = modifiers.Get(id) == nullptr;

				Report("swap-ends-field-medic-buff", started && ended,
					"id=" + std::to_string(id) + " started=" + std::to_string(started) + " ended=" + std::to_string(ended));
			}

			// ZelemBasicMelee's AI definition names its passive, which no script requests.
			void CheckMobPassiveStartsOnce() {
				auto mob = mGame.GetObjectManager().Create(utils::hash_id(passiveNounName));
				if (!mob || !mob->GetNoun()) {
					Report("mob-passive-starts-once", false, "no ZelemBasicMelee noun");
					return;
				}

				Game::PassiveModifiers passives(mLua);
				const uint32_t first = passives.Start(mob);
				const uint32_t second = passives.Start(mob);

				const auto running = Manager().GetAll(mob->GetId());
				const bool isPassive = running.size() == 1 &&
					Manager().Get(running.front())->spec.guid == utils::hash_id("ZelemBasicMeleePassive");
				Report("mob-passive-starts-once", isPassive && first != 0 && second == 0,
					"running=" + std::to_string(running.size()) + " second=" + std::to_string(second) +
					" named=" + std::to_string(Game::PassiveModifiers::NamedBy(*mob->GetNoun())) +
					" ai=" + std::to_string(mob->GetNoun()->GetAIDefinition() != nullptr));
			}

			// Beams (twin laser, plasma wall) ask for what lies along a line, with the game's own call.
			void CheckLineFindsMobOnIt() {
				auto mob = Spawn();
				if (!mob) {
					Report("line-finds-mob-on-it", false, "no mob");
					return;
				}
				const auto at = mob->GetPosition();
				sol::protected_function along = mState["nObjectManager"]["GetObjectsAlongLine"];
				auto countOn = [&](float sideways) -> size_t {
					sol::protected_function_result result = along(at.x - 10, at.y + sideways, at.z + 0.5f, at.x + 10, at.y + sideways, at.z + 0.5f, 0);
					if (!result.valid() || result.get_type() != sol::type::table) {
						return SIZE_MAX;
					}
					size_t found = 0;
					for (const auto& entry : result.get<sol::table>()) {
						found += entry.second.as<uint32_t>() == mob->GetId();
					}
					return found;
				};

				// New objects reach the spatial index on a later update, which can take a few.
				size_t onLine = 0;
				for (int waited = 0; waited < 500 && onLine == 0; waited += 20) {
					Wait(20);
					onLine = countOn(0);
				}
				const size_t beside = countOn(20);
				Report("line-finds-mob-on-it", onLine == 1 && beside == 0,
					"on=" + std::to_string(onLine) + " beside=" + std::to_string(beside));
				mob->MarkForDeletion();
			}

			// Bosses and pets end through nGameObject.KillObject, which must die like a killing hit.
			void CheckKillObjectEndsPassive() {
				auto mob = mGame.GetObjectManager().Create(utils::hash_id(passiveNounName));
				if (!mob || !mob->GetNoun() || Game::PassiveModifiers(mLua).Start(mob) == 0) {
					Report("kill-object-ends-passive", false, "no passive to end");
					return;
				}

				sol::protected_function kill = mState["nGameObject"]["KillObject"];
				const bool called = kill(mob->GetId()).valid();
				mGame.GetModifiers().Update();

				const auto left = Manager().GetAll(mob->GetId()).size();
				Report("kill-object-ends-passive", called && mob->GetHealth() <= 0 && left == 0,
					"health=" + std::to_string(mob->GetHealth()) + " left=" + std::to_string(left));
			}

			// The nomad drone passive makes its drone with nObjectManager.CreateCreature and gives it an owner.
			void CheckDronePassiveSpawnsDrone() {
				auto agent = Spawn();
				sol::protected_function spid = mState["nUtil"]["SPID"];
				const auto id = Request(agent, agent, spid("NomadWithDronePassive"));
				Wait(100);

				const uint32_t droneNoun = utils::hash_id("NomadDrone.Noun");
				size_t drones = 0;
				size_t owned = 0;
				for (const auto& object : mGame.GetObjectManager().GetActiveObjects()) {
					if (object->GetNoun() && object->GetNoun()->GetId() == droneNoun) {
						++drones;
						owned += object->GetOwnerObject() == agent;
					}
				}
				Report("drone-passive-spawns-drone", id != 0 && drones == 1 && owned == 1,
					"id=" + std::to_string(id) + " drones=" + std::to_string(drones) + " owned=" + std::to_string(owned));
			}

			bool HasModifier(const Game::ObjectPtr& object, uint32_t guid) {
				for (uint32_t id : Manager().GetAll(object->GetId())) {
					const auto instance = Manager().Get(id);
					if (instance && instance->spec.guid == guid && !instance->markedForDelete) {
						return true;
					}
				}
				return false;
			}

			// The game's Zelem slow aura puts its slow on an enemy inside its trigger volume and takes
			// it off when the enemy leaves. Its own agent is never slowed, and the volume goes with it.
			// modifier_stealth shows its agent again when the agent uses an ability.
			void CheckStealthEndsOnUsedAbility() {
				auto agent = Spawn();
				sol::protected_function spid = mState["nUtil"]["SPID"];
				const auto id = Request(agent, agent, spid("StealthModifier"));
				Wait(100);
				const auto hidden = agent->GetStealthType();

				mGame.ReportUsedAbility(agent->GetId(), 0);
				Wait(100);
				const auto after = agent->GetStealthType();

				Report("stealth-ends-on-used-ability", id != 0 && hidden != Game::StealthType::None && after == Game::StealthType::None,
					"id=" + std::to_string(id) + " hidden=" + std::to_string(static_cast<int>(hidden)) +
					" after=" + std::to_string(static_cast<int>(after)));
			}

			// A cast that stealths its caster must not be broken by its own UsedAbility event, but the
			// next ability the agent uses shows it again.
			void CheckStealthCastKeepsItsStealth() {
				auto agent = Spawn();
				const auto stealthId = utils::hash_id("ModifierCheckStealthCast");
				const auto waitsId = utils::hash_id("ModifierCheckWaits");
				mLua.GetAbility(stealthId)->FindNumberedTick();
				mLua.GetAbility(waitsId)->FindNumberedTick();

				RakNet::CombatData cast {};
				cast.abilityId = stealthId;
				cast.abilityRank = 1;
				mGame.UseAbility(agent, cast);
				Wait(100);
				const auto hidden = agent->GetStealthType();

				cast.abilityId = waitsId;
				mGame.UseAbility(agent, cast);
				Wait(100);
				const auto after = agent->GetStealthType();

				Report("stealth-cast-keeps-its-stealth", hidden != Game::StealthType::None && after == Game::StealthType::None,
					"hidden=" + std::to_string(static_cast<int>(hidden)) + " after=" + std::to_string(static_cast<int>(after)));
			}

			// Vex's time bubble (SpacetimeRandom2, ability_delaying_sphere) is an object that lasts 12
			// seconds and then deletes itself. Runs the real ability, so this check takes about 14 s.
			void CheckTimeBubbleEnds() {
				auto agent = Spawn();
				const auto ability = mLua.GetAbility(utils::hash_id("SpacetimeRandom2"));
				if (!agent || !ability) {
					Report("time-bubble-ends", false, "no agent or ability");
					return;
				}

				const auto others = [this, &agent] {
					size_t count = 0;
					for (const auto& object : mGame.GetObjectManager().GetObjectsInRadius(Game::BoundingSphere(agent->GetPosition(), 30.f), {})) {
						count += object != agent && !object->IsMarkedForDeletion();
					}
					return count;
				};

				const auto before = others();
				RakNet::CombatData cast {};
				cast.abilityId = utils::hash_id("SpacetimeRandom2");
				cast.abilityRank = 1;
				cast.cursorPosition = agent->GetPosition();
				mGame.UseAbility(agent, cast);
				Wait(1500);
				const auto during = others();
				Wait(12500);
				const auto after = others();

				Report("time-bubble-ends", during > before && after == before,
					"before=" + std::to_string(before) + " during=" + std::to_string(during) + " after=" + std::to_string(after));
			}

			// Blitz's Electron Sphere is a squad ability shot through the projectile template, which
			// reads the hero's shared ability offset as three numbers. The sphere is an object.
			void CheckElectronSphereFlies() {
				auto agent = Spawn();
				const auto count = [this, &agent] {
					size_t objects = 0;
					for (const auto& object : mGame.GetObjectManager().GetObjectsInRadius(Game::BoundingSphere(agent->GetPosition(), 60.f), {})) {
						objects += !object->IsMarkedForDeletion();
					}
					return objects;
				};

				const auto before = count();
				RakNet::CombatData cast {};
				cast.abilityId = utils::hash_id("PlasmaRandom_LightningBall");
				cast.abilityRank = 1;
				cast.cursorPosition = agent->GetPosition() + glm::vec3(10.f, 0.f, 0.f);
				mGame.UseAbility(agent, cast);
				Wait(1000);
				const auto during = count();
				Report("electron-sphere-flies", during > before, "before=" + std::to_string(before) + " during=" + std::to_string(during));
			}

			void CheckAuraSlowsEnemyInside() {
				auto* volumes = mGame.GetTriggerVolumes();
				const size_t volumesBefore = volumes ? volumes->Count() : 0;

				// Away from the other checks' mobs, which sit at the origin.
				const glm::vec3 spot(500.f, 500.f, 0.f);
				auto agent = Spawn();
				auto enemy = Spawn();
				agent->SetTeam(1);
				enemy->SetTeam(2);
				agent->SetPosition(spot);
				enemy->SetPosition(spot + glm::vec3(5.f, 0.f, 0.f));

				sol::protected_function spid = mState["nUtil"]["SPID"];
				const uint32_t slow = spid("ZelemSlow");
				const auto id = Request(agent, agent, spid("ZelemSlowAura"));
				Wait(100);
				const bool inside = HasModifier(enemy, slow);
				const bool agentSlowed = HasModifier(agent, slow);

				enemy->SetPosition(spot + glm::vec3(60.f, 0.f, 0.f));
				Wait(100);
				const bool outside = HasModifier(enemy, slow);

				agent->MarkForDeletion();
				Wait(100);
				const size_t volumesAfter = volumes ? volumes->Count() : 0;

				Report("aura-slows-enemy-inside", id != 0 && inside && !agentSlowed && !outside && volumesAfter == volumesBefore,
					"id=" + std::to_string(id) + " inside=" + std::to_string(inside) + " agent=" + std::to_string(agentSlowed) +
					" outside=" + std::to_string(outside) + " volumes=" + std::to_string(volumesBefore) + "->" + std::to_string(volumesAfter));
			}

			// A script's jump, as charges and knockbacks make them: the object rises on the way and
			// lands its distance away, then stops jumping.
			void CheckJumpLandsWhereItAimed() {
				auto object = Spawn();
				const glm::vec3 start(-500.f, 500.f, 0.f);
				object->SetPosition(start);

				// 10 along +X at 20 a second, 1 to 3 high, the highest from 5 away.
				sol::protected_function jump = mState["nLocomotion"]["JumpInDirection"];
				sol::protected_function_result started = jump(object->GetId(), 1, 0, 0, 10, 20, 1, 0, 0, 1, 3, 5);
				const bool began = started.valid() && started.get_type() == sol::type::boolean && started.get<bool>();

				Wait(240);
				const auto& middle = object->GetPosition();
				const bool rose = middle.z > start.z + 1.f && middle.x > start.x + 2.f && middle.x < start.x + 9.f;
				Wait(500);
				const auto& end = object->GetPosition();
				const bool landed = std::abs(end.x - (start.x + 10.f)) < 0.01f && std::abs(end.z - start.z) < 0.01f;
				const bool jumping = object->GetLocomotionData() && object->GetLocomotionData()->IsJumping();

				Report("jump-lands-where-it-aimed", began && rose && landed && !jumping,
					"began=" + std::to_string(began) + " rose=" + std::to_string(rose) + " landed=" + std::to_string(landed) +
					" end=" + std::to_string(end.x - start.x) + "," + std::to_string(end.z - start.z));
				object->MarkForDeletion();
			}

			// The game's own thorns script, given a melee hit, hits the attacker back.
			void CheckThornsHitsMeleeAttackerBack() {
				auto victim = Spawn();
				auto attacker = Spawn();

				sol::protected_function spid = mState["nUtil"]["SPID"];
				const uint32_t guid = spid("ThornsModifier");
				const auto id = Request(victim, victim, guid);

				const float before = attacker->GetHealth();
				Game::DamageReport hit;
				hit.victim = victim->GetId();
				hit.attacker = attacker->GetId();
				hit.amount = 100.f;
				hit.descriptors = mState["nDescriptors"]["IsMelee"].get_or(0u);
				mGame.ReportDamage(hit);
				Wait(40);

				const float after = attacker->GetHealth();
				Report("thorns-hits-melee-attacker-back", id != 0 && after < before,
					"id=" + std::to_string(id) + " attacker=" + std::to_string(before) + "->" + std::to_string(after));
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

				// Ids are never reused now (a stale id cannot reach a new object), so this checks that a
				// modifier whose object went away takes nothing from the next object at its end.
				Report("removed-object-leaves-new-changes", reused->GetId() != otherId && Strength(reused) == before + 5,
					"newId=" + std::to_string(reused->GetId() != otherId) + " before=" + std::to_string(before) + " after=" + std::to_string(Strength(reused)));
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
				const float expected = std::max(0.f, 5.f - first->GetFootprintRadius() - second->GetFootprintRadius());

				Report("distance-can-be-read", value && std::abs(*value - expected) < 0.001f,
					"distance=" + std::to_string(value.value_or(-1.f)) + " expected=" + std::to_string(expected));
			}

			// A cast the player cancels still runs its Deactivate step, so its stat change comes off.
			void CheckCancelledCastUndoesItsChanges(const char* abilityName, const char* checkName) {
				auto agent = Spawn();
				const auto ability = mLua.GetAbility(utils::hash_id(abilityName));
				ability->FindNumberedTick();

				const float before = agent->GetAttributeValue(Game::AttributeType::Strength);
				ability->Tick(agent, nullptr, glm::vec3(0.f), 1);
				const float during = agent->GetAttributeValue(Game::AttributeType::Strength);

				mLua.CancelThread(agent->GetLuaThread());
				const float after = agent->GetAttributeValue(Game::AttributeType::Strength);
				sol::table cancel = mState["ModifierCheckCancel"];
				const bool ended = cancel["ended"].get_or(false);
				cancel["ended"] = false;

				Report(checkName, during > before && after == before && ended && !agent->GetLuaThread(),
					"before=" + std::to_string(before) + " during=" + std::to_string(during) + " after=" + std::to_string(after) +
					" ended=" + std::to_string(ended));
			}

			// The client holds a cast's changes outside the script, so a Deactivate that dies cannot keep them.
			void CheckFailedDeactivateUndoesItsChanges() {
				auto agent = Spawn();
				const auto ability = mLua.GetAbility(utils::hash_id("ModifierCheckDeactivateFails"));
				ability->FindNumberedTick();

				const float before = agent->GetAttributeValue(Game::AttributeType::Strength);
				ability->Tick(agent, nullptr, glm::vec3(0.f), 1);
				const float during = agent->GetAttributeValue(Game::AttributeType::Strength);

				mLua.CancelThread(agent->GetLuaThread());
				const float after = agent->GetAttributeValue(Game::AttributeType::Strength);

				Report("failed-deactivate-undoes-its-changes", during > before && after == before,
					"before=" + std::to_string(before) + " during=" + std::to_string(during) + " after=" + std::to_string(after));
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
					// A Tick starts on the update after Activate (as in the client); run it to its
					// first wait so its missing functions are reported too.
					mGame.GetLua().Update();
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
