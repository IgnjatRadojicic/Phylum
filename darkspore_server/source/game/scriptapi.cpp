
// Include
#include "instance.h"
#include "lua.h"
#include "modifiersystem.h"
#include "object.h"
#include "objectmanager.h"
#include "scriptgaps.h"
#include "scriptsource.h"

#include "utils/functions.h"

#include <fstream>
#include <memory>

// The game's scripts refer to objects by id and expect the original client's Lua API.
// This file adapts those calls to the functions this server already has. It should only
// translate: real behaviour belongs in C++.
namespace {
	constexpr const char* setupApi = R"lua(
		setmetatable(nAbility, { __index = Ability })
		setmetatable(nLocomotion, { __index = Locomotion })

		nProjectileFlags = { kHoming = 1 }

		-- Approximate: objects move every 50 milliseconds, give or take a little
		nTimeManager = {}
		function nTimeManager.GetSimTimeDt()
			return 0.05
		end

		-- Scripts ask for a modifier by id. They add its functions after registering it, so these
		-- stay in the table until a modifier runs.
		nModifier = nModifier or {}
		nModifier.registered = {}
		function nModifier.RegisterModifier(name, modifier)
			nModifier.registered[nUtil.SPID(name)] = modifier
		end
		function nModifier.PreloadAsset(name)
			return nUtil.GetAsset(name)
		end

		-- Scripts require files as "Folder!Name.lua"
		table.insert(package.loaders, 2, function(name)
			local folder, file = name:match("^([^!]+)!(.+)%.lua$")
			if not file then
				folder, file = "Lua", name
			end
			local directory = folder
			if folder == "Lua" or folder:match("^0x") then
				directory = "lua"
			end
			local path = "data/serverdata/" .. directory .. "/" .. file .. ".lua"
			local source = io.open(path) and ReadGameScript(path)
			if not source then
				return nil
			end
			return loadstring(source, "@" .. path)
		end)
	)lua";

	// A missing name reads as nil, and the script only fails later, somewhere else. Watching
	// every engine table and the globals names the real gap. Values do not change.
	constexpr const char* watchApi = R"lua(
		local function watch(label, target)
			local meta = getmetatable(target)
			if meta == nil then
				meta = {}
				setmetatable(target, meta)
			elseif type(meta) ~= "table" then
				return
			end

			local fallback = meta.__index
			meta.__index = function(self, key)
				local value
				if type(fallback) == "function" then
					value = fallback(self, key)
				elseif fallback ~= nil then
					value = fallback[key]
				end
				if value == nil then
					ScriptGaps.Record(label .. "." .. tostring(key))
				end
				return value
			end
		end

		for name, value in pairs(_G) do
			if type(value) == "table" and name:match("^n%u") then
				watch(name, value)
			end
		end
		watch("global", _G)
	)lua";

	// Each running ability is a coroutine, so its context follows it across waits.
	constexpr const char* castApi = R"lua(
		local running = coroutine.running
		local casts = setmetatable({}, { __mode = "k" })
		local sequenceSteps = {}
		local nextInstanceId = 0

		-- The main thread is no coroutine, so running gives nil there.
		local function currentThread()
			return running() or casts
		end

		local function currentCast()
			return casts[currentThread()]
		end

		-- A thread started by an ability belongs to the same cast.
		function nThread.CreateThreadForObject(id, callback, ...)
			local cast = currentCast()
			nThread.Create(id, function(...)
				casts[running()] = cast
				return callback(...)
			end, ...)
		end

		function nAbility.BeginCast(ability, object, target, cursorPosition, rank)
			nextInstanceId = nextInstanceId + 1
			local id = object:GetId()
			ScriptTrace("CAST ability=" .. tostring(ability.id) .. " object=" .. tostring(id))
			local steps = ability.animationSequence and #ability.animationSequence or 0
			local step = sequenceSteps[id] or 0
			if steps > 0 then
				sequenceSteps[id] = (step + 1) % steps
			end
			casts[running()] = {
				ability = ability,
				object = object,
				target = target,
				cursorPosition = cursorPosition,
				rank = rank or 1,
				sequenceIndex = step,
				instanceId = nextInstanceId
			}
		end

		-- A script can call its own tick to reuse it, which is not a new cast
		local function castingTick(tick)
			return function(self, object, target, cursorPosition, rank)
				if object then
					nAbility.BeginCast(self, object, target, cursorPosition, rank)
				end
				return tick(self)
			end
		end

		nAbility.scripts = {}
		local registerAbility = nAbility.RegisterAbility
		function nAbility.RegisterAbility(name, ability)
			nAbility.scripts[name] = ability
			local tick = ability.tick
			if tick then
				ability.tick = castingTick(tick)
			end
			registerAbility(name, ability)
		end

		-- Scripts with no template set tick by its nAbilityFns number, after they register
		function nAbility.NumberedTick(ability)
			local tick = ability[nAbilityFns.Tick]
			if tick then
				ability.tick = castingTick(tick)
				return ability.tick
			end
		end

		function nAbility.GetAgentID()
			return currentCast().object:GetId()
		end

		-- A modifier's caster is whoever started it, not the object it sits on
		function nAbility.GetCaster()
			local cast = currentCast()
			return cast and (cast.initiator or cast.object) or nil
		end

		CastContext = {
			Get = currentCast,
			Set = function(context)
				casts[currentThread()] = context
			end
		}

		function nAbility.GetTargetID()
			local target = currentCast().target
			return target and target:GetId() or kObjIDNone
		end

		function nAbility.GetTargetPosition()
			local cast = currentCast()
			local position = cast.target and cast.target:GetPosition() or cast.cursorPosition
			return position.x, position.y, position.z
		end

		function nAbility.GetAgentAttributeSnapshot()
			return currentCast().object:GetAttributeSnapshot()
		end

		function nAbility.GetAbilityInstanceID()
			return currentCast().instanceId
		end

		function nAbility.GetAnimationSequenceIndex()
			return currentCast().sequenceIndex
		end

		function nAbility.GetRank()
			return currentCast().rank
		end

		function nAbility.GetRankedValue(value)
			return GetRankedValueHelper(value, currentCast().rank)
		end

		function nAbility.TargetInRangeAtStart()
			return true
		end

		function nAbility.GetTimeToReleaseLua(ability)
			local sequence = ability.animationSequence
			local step = sequence and sequence[currentCast().sequenceIndex + 1]
			return step and step.release or 0
		end

		function nAbility.PlayAnimationSequence()
			local cast = currentCast()
			local step = cast.ability.animationSequence[cast.sequenceIndex + 1]
			if step then
				cast.object:SetAnimationState(step.animationstate)
			end
		end

		-- Cooldowns are sent by the server's ability reply for now
		-- Private storage for whatever is running, kept across waits
		nThreadData = {}
		local looseData = setmetatable({}, { __mode = "k" })

		local function dataOwner()
			local thread = running() or casts
			local cast = casts[thread]
			if cast then
				return cast
			end
			looseData[thread] = looseData[thread] or {}
			return looseData[thread]
		end

		function nThreadData.CreatePrivateTable()
			local owner = dataOwner()
			owner.privateTable = {}
			return owner.privateTable
		end

		function nThreadData.GetPrivateTable()
			local owner = dataOwner()
			owner.privateTable = owner.privateTable or {}
			return owner.privateTable
		end

		local function setData(key, value)
			nThreadData.GetPrivateTable()[key] = value
		end

		local function getData(key)
			return nThreadData.GetPrivateTable()[key]
		end

		nThreadData.SetInt, nThreadData.GetInt = setData, getData
		nThreadData.SetFloat, nThreadData.GetFloat = setData, getData
		nThreadData.SetGUID, nThreadData.GetGUID = setData, getData

		function nDebug.IsAbilityDebugEnabled()
			return false
		end

		function nAbility.PayCooldownAndMana() end
		function nAbility.DebugDrawDamageArc() end
		function nDebug.Assert() end
		function nDebug.LogToConsole() end
		function nDebug.DrawLine() end

		nEvent = nEvent or {}
		function nEvent.Notify() end
	)lua";

	constexpr const char* objectApi = R"lua(
		local function get(id)
			return id and id ~= kObjIDNone and nObjectManager.GetObject(id) or nil
		end

		local function unpackVector(vector)
			return vector.x, vector.y, vector.z
		end

		-- Anything not listed below takes an id and matches a server Object method by name.
		setmetatable(nGameObject, {
			__index = function(_, name)
				return function(id, ...)
					local object = get(id)
					local method = object and object[name]
					if method then
						return method(object, ...)
					end
					if object then
						ScriptGaps.Record("nGameObject." .. name)
					end
				end
			end
		})

		function nGameObject.GetPosition(id)
			local object = get(id)
			if object then
				return unpackVector(object:GetPosition())
			end
			return 0, 0, 0
		end

		function nGameObject.GetFacing(id)
			local object = get(id)
			if object then
				return unpackVector(object:GetFacing())
			end
			return 0, 0, 0
		end
		nGameObject.GetObjectDirection = nGameObject.GetFacing

		function nGameObject.GetCenterPoint(id)
			local object = get(id)
			if object then
				return unpackVector(object:GetCenterPoint())
			end
			return 0, 0, 0
		end

		-- Scripts never read these four values, only hand them back to the functions here.
		function nGameObject.GetOrientation(id)
			local object = get(id)
			if object then
				local orientation = object:GetOrientation()
				return orientation.x, orientation.y, orientation.z, orientation.w
			end
			return 0, 0, 0, 1
		end

		function nGameObject.SetOrientation(id, x, y, z, w)
			local object = get(id)
			if object then
				object:SetOrientation(quat.new(w or 1, x or 0, y or 0, z or 0))
			end
		end

		-- Scripts give an animation as a name or an id
		function nGameObject.SetAnimationState(id, animation, ...)
			local object = get(id)
			if object then
				if type(animation) == "string" then
					animation = nUtil.GetAsset(animation)
				end
				return object:SetAnimationState(animation, ...)
			end
		end

		function nGameObject.MarkForDelete(id)
			local object = get(id)
			if object then
				object:MarkForDeletion()
			end
		end

		function nGameObject.SetOwnerID(id, ownerId)
			local object = get(id)
			if object then
				object:SetOwnerObject(get(ownerId))
			end
		end

		function nGameObject.GetHitPoints(id)
			local object = get(id)
			return object and object:GetHealth() or 0
		end

		function nGameObject.GetLinearVelocity()
			return 0, 0, 0
		end

		function nGameObject.GetNPCType(id)
			local object = get(id)
			return object and object:GetNpcType() or 0
		end

		function nGameObject.GetOwnerID(id)
			local object = get(id)
			local owner = object and object:GetOwnerObject()
			return owner and owner:GetId() or kObjIDNone
		end

		function nGameObject.IsPet()
			return false
		end

		function nGameObject.IsAlive(id)
			local object = get(id)
			return object ~= nil and object:GetHealth() > 0
		end

		function nGameObject.ValidateHostileTarget(agentId, targetId)
			local agent, target = get(agentId), get(targetId)
			return agent ~= nil and target ~= nil and target:GetHealth() > 0 and agent:GetTeam() ~= target:GetTeam()
		end

		function nGameObject.ValidateFriendlyTarget(agentId, targetId)
			local agent, target = get(agentId), get(targetId)
			return agent ~= nil and target ~= nil and target:GetHealth() > 0 and agent:GetTeam() == target:GetTeam()
		end

		function nGameObject.TakeDamage(snapshot, targetId, damage, damageType, damageSource, coefficient, descriptors, multiplier, x, y, z)
			local target = get(targetId)
			if not target then
				return false, 0, false
			end
			local direction = x and vec3.new(x, y, z) or nil
			local attacker = nAbility.GetCaster() or target
			return target:TakeDamage(attacker, snapshot or false, damage, damageType or 0, damageSource or 0, coefficient or 0, descriptors or 0, multiplier, direction)
		end

		nAttribute = nAttribute or {}
		function nAttribute.GetAttributeValue(id, attribute)
			local object = get(id)
			return object and object:GetAttributeValue(ModifierEngine.AttributeFromScript(attribute)) or 0
		end

		function nAttribute.GetPrimaryAttributeValue(id)
			local object = get(id)
			return object and object:GetPrimaryAttributeValue() or 0
		end

		nPlayer = nPlayer or {}
		function nPlayer.IsPlayerControlledObject(id)
			local object = get(id)
			return object ~= nil and object:IsPlayerControlled()
		end

		function nObjectManager.IsValidObject(id)
			return get(id) ~= nil
		end

		nGameDirector = nGameDirector or {}
		function nGameDirector.IsHordeActive()
			return false
		end
	)lua";

	// The game's scripts pass positions as x, y, z. Server functions take and return a vec3.
	// Modifiers are scripts like abilities, and run with the same kind of context. The rules for
	// them live in ModifierManager, and ModifierSystem runs them.
	constexpr const char* modifierApi = R"lua(
		local function get(id)
			return id and id ~= kObjIDNone and nObjectManager.GetObject(id) or nil
		end

		local function contextOf(id)
			if id == nil then
				return CastContext.Get()
			end
			return ModifierEngine.GetContext(id)
		end

		local function instanceOf(id)
			if id ~= nil then
				return id
			end
			local context = CastContext.Get()
			return context and context.modifierId or 0
		end

		-- A script can set a function by its nAbilityFns number or by its name
		local function scriptFunction(modifier, number, name, flag)
			if modifier[flag] == false then
				return nil
			end
			return modifier[number] or modifier[name]
		end

		ModifierApi = {}

		function ModifierApi.Start(context)
			CastContext.Set(context)
			local modifier = context.ability
			local activate = scriptFunction(modifier, nAbilityFns.Activate, "activate", "hasActivate")
			if activate then
				activate(modifier)
			end
			local tick = scriptFunction(modifier, nAbilityFns.Tick, "tick", "hasTick")
			if tick then
				tick(modifier)
			end
		end

		function ModifierApi.Stop(context)
			CastContext.Set(context)
			local modifier = context.ability
			local deactivate = scriptFunction(modifier, nAbilityFns.Deactivate, "deactivate", "hasDeactivate")
			if deactivate then
				deactivate(modifier)
			end
		end

		-- The event reads as the modifier too, because handlers use their one argument as both
		function ModifierApi.Stack(context)
			CastContext.Set(context)
			local modifier = context.ability
			local handler = scriptFunction(modifier, nAbilityFns.HandleEvent, "handleEvent", "hasHandleEvent")
			if handler then
				local event = setmetatable({ eventType = nAbilityEventFlags.StackModifier }, { __index = modifier })
				context.handled = handler(event) == true
			end
		end

		function nAbility.GetAbilityEventType(event)
			return event and event.eventType
		end

		local function canUse(modifier, agent, initiator, rank)
			if modifier.CanUseModifier then
				return modifier.CanUseModifier(agent, initiator, rank) ~= false
			elseif modifier.canUseModifier then
				return modifier:canUseModifier(agent, initiator, rank) ~= false
			end
			return true
		end

		local function hasFlag(flags, flag)
			return flag ~= nil and nBit.And(flags or 0, flag) ~= 0
		end

		local function hasAnyFlag(flags, names)
			for _, name in ipairs(names) do
				if hasFlag(flags, nDebuffDescriptors[name]) then
					return true
				end
			end
			return false
		end

		-- Approximate: which debuffs count as crowd control or block actions in the client
		local boundDebuffs = { "IsStun", "IsFear", "IsBanish", "IsSleep", "IsShock" }
		local crowdControlDebuffs = { "IsStun", "IsFear", "IsBanish", "IsSleep", "IsShock", "IsRoot", "IsSlow", "IsSilence", "IsTaunt" }

		function nModifier.RequestModifier(agent, initiator, guid, snapshot, rank, ...)
			local modifier = nModifier.registered[guid]
			local object = get(agent)
			if not modifier or not object then
				ScriptTrace("MODIFIER not-found guid=" .. tostring(guid) .. " known=" .. tostring(modifier ~= nil) .. " agent=" .. tostring(agent))
				return kObjIDNone
			end

			rank = rank or 1

			local caster = get(initiator)
			local source = caster or object
			local debuffs = modifier.debuffDescriptors
			local descriptors = modifier.descriptors
			local context = {
				ability = modifier,
				object = object,
				initiator = caster,
				initiatorId = initiator or kObjIDNone,
				guid = guid,
				rank = rank,
				snapshot = snapshot or source:GetAttributeSnapshot(),
				properties = { ... },
				descriptors = descriptors or 0,
				debuffDescriptors = debuffs or 0
			}

			-- It can read the request's properties, so it runs as the modifier it may become
			local previous = CastContext.Get()
			CastContext.Set(context)
			local allowed = canUse(modifier, agent, initiator, rank)
			CastContext.Set(previous)
			if not allowed then
				ScriptTrace("MODIFIER can-use-refused guid=" .. tostring(guid) .. " agent=" .. tostring(agent))
				return kObjIDNone
			end

			local id = ModifierEngine.Request(context, {
				guid = guid,
				seconds = GetRankedValueHelper(modifier.duration or 0, rank) or 0,
				maxStackCount = GetRankedValueHelper(modifier.maxStackCount or 0, rank) or 0,
				activation = modifier.activationType or 0,
				deactivation = modifier.deactivationType or 0,
				bound = hasAnyFlag(debuffs, boundDebuffs),
				isBuff = hasFlag(descriptors, nDescriptors.IsBuff),
				isDebuff = hasFlag(descriptors, nDescriptors.IsDebuff),
				isDoT = hasFlag(descriptors, nDescriptors.IsDoT),
				isCrowdControl = hasAnyFlag(debuffs, crowdControlDebuffs)
			}, agent, initiator or kObjIDNone)
			return id ~= 0 and id or kObjIDNone
		end

		function nModifier.GetMyAgentID()
			return CastContext.Get().object:GetId()
		end

		function nModifier.GetMyInitiatorID()
			return CastContext.Get().initiatorId
		end

		function nModifier.GetInitiatorID(id)
			local context = contextOf(id)
			return context and context.initiatorId or kObjIDNone
		end

		function nModifier.GetInitiatorAttributeSnapshot(id)
			local context = contextOf(id)
			return context and context.snapshot
		end

		function nModifier.GetModifierInstanceID()
			return instanceOf(nil)
		end

		function nModifier.GetModifierGUID(id)
			local context = contextOf(id)
			return context and context.guid or 0
		end

		function nModifier.GetRank(id)
			local context = contextOf(id)
			return context and context.rank or 1
		end

		-- Numbered from zero. The first argument is the modifier itself.
		function nModifier.GetFloatProperty(_, index)
			return CastContext.Get().properties[index + 1]
		end

		function nModifier.GetStackCount(id)
			return ModifierEngine.GetStackCount(instanceOf(id))
		end

		function nModifier.GetMyStackCount()
			return ModifierEngine.GetStackCount(instanceOf(nil))
		end

		function nModifier.SetStackCount(count)
			ModifierEngine.SetStackCount(instanceOf(nil), count)
		end

		function nModifier.IncrementStackCount()
			local id = instanceOf(nil)
			ModifierEngine.SetStackCount(id, ModifierEngine.GetStackCount(id) + 1)
			return ModifierEngine.GetStackCount(id)
		end

		function nModifier.ResetDuration()
			ModifierEngine.ResetDuration(instanceOf(nil))
		end

		function nModifier.GetRemainingDuration(id)
			return ModifierEngine.GetRemainingSeconds(instanceOf(id))
		end

		function nModifier.IsModifierRunning(id)
			return id ~= nil and ModifierEngine.GetContext(id) ~= nil
		end

		-- Scripts pass the agent and the instance, or only the instance, or nothing for themselves.
		-- They often look the instance up first, so one that is not running means nothing to end.
		function nModifier.MarkForDelete(...)
			local count = select("#", ...)
			local id
			if count == 0 then
				id = instanceOf(nil)
			else
				id = select(count, ...)
			end
			if type(id) == "number" and ModifierEngine.GetContext(id) ~= nil then
				ModifierEngine.MarkForDelete(id)
			end
		end

		function nModifier.GetFirstModifierByGUID(agent, guid)
			local id = ModifierEngine.GetFirst(agent or 0, guid or 0)
			return id ~= 0 and id or kObjIDNone
		end

		function nModifier.AgentHasModifierMatchingGUID(agent, guid)
			return ModifierEngine.GetFirst(agent or 0, guid or 0) ~= 0
		end
		nGameObject.IsModifierActive = nModifier.AgentHasModifierMatchingGUID

		local function matching(agent, field, flag)
			local found = {}
			for _, id in ipairs(ModifierEngine.GetAll(agent or 0)) do
				local context = ModifierEngine.GetContext(id)
				if context and hasFlag(context[field], flag) then
					found[#found + 1] = id
				end
			end
			return found
		end

		function nModifier.GetModifiersMatchingDescriptor(agent, flag)
			return matching(agent, "descriptors", flag)
		end

		function nModifier.CheckForModifierDebuffDescriptor(agent, flag)
			return #matching(agent, "debuffDescriptors", flag) > 0
		end

		-- Runs a function as if it were inside that modifier, then goes back
		local function finishInContext(previous, ...)
			CastContext.Set(previous)
			return ...
		end

		function nModifier.CallFunctionInContext(id, callback, ...)
			local previous = CastContext.Get()
			CastContext.Set(contextOf(id) or previous)
			return finishInContext(previous, callback(...))
		end
		nAbility.CallFunctionInContext = nModifier.CallFunctionInContext

		-- Without an operation a change adds. Inside a modifier it ends with the modifier.
		function nAttribute.AddAttributeModifier(id, attribute, value, operation)
			return ModifierEngine.AddAttributeChange(id, attribute, value or 0, operation or nAttributeOp.kAdd, instanceOf(nil))
		end

		function nAttribute.RemoveAttributeModifier(id, handle)
			if handle then
				ModifierEngine.RemoveAttributeChange(id, handle, instanceOf(nil))
			end
		end
	)lua";

	constexpr const char* vectorApi = R"lua(
		local function get(id)
			return id and id ~= kObjIDNone and nObjectManager.GetObject(id) or nil
		end

		local function vector(x, y, z)
			return vec3.new(x or 0, y or 0, z or 0)
		end

		local function toIds(objects)
			local ids = {}
			for index, object in ipairs(objects) do
				ids[index] = object:GetId()
			end
			return ids
		end

		for _, name in ipairs({ "GetObjectsInRadius", "GetObjectsInRadius_SortedByDistance" }) do
			local query = nObjectManager[name]
			nObjectManager[name] = function(x, y, z, radius, types)
				return toIds(query(vector(x, y, z), radius, type(types) == "table" and types or {}))
			end
		end

		function nObjectManager.RemoveObjectById(id)
			local object = get(id)
			if object then
				object:MarkForDeletion()
			end
		end

		local createObject = nObjectManager.CreateObject
		function nObjectManager.CreateObject(noun, x, y, z, ...)
			local object = createObject(noun, vector(x, y, z))
			if not object then
				return kObjIDNone
			end
			if select("#", ...) >= 4 then
				nGameObject.SetOrientation(object:GetId(), ...)
			end
			return object:GetId()
		end

		local rotateVector = nMathUtil.RotateVectorByAxisAngle
		function nMathUtil.RotateVectorByAxisAngle(x, y, z, axisX, axisY, axisZ, angle)
			local result = rotateVector(vector(x, y, z), vector(axisX, axisY, axisZ), angle)
			return result.x, result.y, result.z
		end

		local circleIntersectsArc = nMathUtil.CircleIntersectsArc
		function nMathUtil.CircleIntersectsArc(x, y, z, radius, ...)
			return circleIntersectsArc(vector(x, y, z), radius, ...)
		end

		-- Scripts give the agent first and read the position as three numbers
		nThread.EngineWaitForProjectile = nThread.WaitForProjectile
		function nThread.WaitForProjectile(agent, projectile, parameters, target, validator)
			local hitId, hitGround, position, remaining = nThread.EngineWaitForProjectile(projectile, parameters, target, validator)
			if hitId == 0 then
				hitId = kObjIDNone
			end
			return hitId, hitGround, position.x, position.y, position.z, remaining
		end

		-- Scripts read where the shot landed as three numbers
		nThread.EngineWaitForLobbedProjectile = nThread.WaitForLobbedProjectile
		function nThread.WaitForLobbedProjectile(agent, projectile, x, y, z, height, flightTime, speed, groundOnly, bounces, restitution, stopOnCreatures)
			local landed = nThread.EngineWaitForLobbedProjectile(projectile, vector(x, y, z), height or 0, flightTime or 0, speed or 0,
				groundOnly ~= false, bounces or 0, restitution or 0, stopOnCreatures == true)
			if landed then
				return landed.x, landed.y, landed.z
			end
			return x, y, z
		end

		local transformVector = nMathUtil.TransformVector
		function nMathUtil.TransformVector(x, y, z, orientationX, orientationY, orientationZ, orientationW)
			local orientation = quat.new(orientationW or 1, orientationX or 0, orientationY or 0, orientationZ or 0)
			local result = transformVector(vector(x, y, z), orientation)
			return result.x, result.y, result.z
		end

		-- Locomotion functions take an object, then a position, then anything else
		for _, name in ipairs({
			"TeleportObject", "JumpInDirection", "SlideToPoint", "MoveToPointExact", "MoveToCircleEdge",
			"MoveToPointWithinRange", "ApplyExternalVelocity", "TurnToFace", "SetTargetPosition"
		}) do
			local move = Locomotion[name]
			nLocomotion[name] = function(id, x, y, z, ...)
				local object = get(id)
				if object then
					return move(object, vector(x, y, z), ...)
				end
			end
		end
		nGameObject.SetTargetPosition = nLocomotion.SetTargetPosition
	)lua";
}

// Game
namespace Game {
	void Lua::SetGapLogPath(const std::string& path) {
		mGapLogPath = path;
	}

	void Lua::SetTracePath(const std::string& path) {
		mTracePath = path;
	}

	void Lua::Trace(const std::string& line) {
		if (!mTrace.is_open()) {
			mTrace.open(mTracePath, std::ios::app);
		}
		mTrace << GetGame().GetTime() << " " << line << std::endl;
	}

	void Lua::RegisterGameScriptApi() {
		sol::table gameAbility = mState["nAbility"];

		gameAbility["RegisterAbility"] = [this](sol::this_state L, const std::string& name, sol::table ability) {
			if (auto [it, inserted] = mAbilities.try_emplace(utils::hash_id(name)); inserted) {
				ability["id"] = it->first;
				it->second = std::make_shared<Ability>(*this, std::move(ability), name, it->first);
			}
		};

		// Scripts pass either a name or an id that is already hashed.
		const auto preload = [](sol::object asset, sol::variadic_args) -> uint32_t {
			return asset.is<uint32_t>() ? asset.as<uint32_t>() : utils::hash_id(asset.as<std::string>());
		};
		gameAbility["PreloadAnimation"] = preload;
		gameAbility["PreloadAsset"] = preload;
		gameAbility["PreloadModifier"] = preload;

		// Scripts keep an asset as an id and hand it back to the engine.
		sol::table util = mState["nUtil"];
		util["GetAsset"] = preload;

		sol::table simulator = mState.create_named_table("nGameSimulator");
		simulator["GetGameTime"] = [this]() {
			return GetGame().GetTime() / 1000.0;
		};
		// A chain is the campaign, as opposed to the arena, and immunities only count in one.
		simulator["IsChainGame"] = [] {
			return true;
		};

		sol::table bitOperations = mState.create_named_table("nBit");
		bitOperations["Or"] = [](sol::variadic_args values) {
			uint32_t result = 0;
			for (uint32_t value : values) {
				result |= value;
			}
			return result;
		};
		bitOperations["And"] = [](sol::variadic_args values) {
			uint32_t result = 0xFFFFFFFF;
			for (uint32_t value : values) {
				result &= value;
			}
			return result;
		};

		mState["ReadGameScript"] = [](const std::string& path) {
			return ScriptSource::Read(path);
		};

		auto gapLog = std::make_shared<std::ofstream>(mGapLogPath, std::ios::app);
		auto gaps = std::make_shared<ScriptGaps>(*gapLog);
		sol::table scriptGaps = mState.create_named_table("ScriptGaps");
		scriptGaps["Record"] = [gapLog, gaps](const std::string& name) {
			gaps->Record(name);
		};

		mState["ScriptTrace"] = [this](const std::string& line) {
			Trace(line);
		};

		sol::table modifierEngine = mState.create_named_table("ModifierEngine");
		modifierEngine["AttributeFromScript"] = [](uint8_t attribute) {
			return AttributeFromScript(attribute);
		};
		modifierEngine["Request"] = [this](sol::table context, sol::table settings, uint32_t agent, uint32_t initiator) {
			ModifierSpec spec;
			spec.guid = settings["guid"];
			spec.maxStackCount = static_cast<uint32_t>(settings.get<double>("maxStackCount"));
			spec.activation = static_cast<ActivationType>(settings.get<uint8_t>("activation"));
			spec.deactivation = static_cast<DeactivationType>(settings.get<uint8_t>("deactivation"));
			spec.bound = settings["bound"];

			const auto& objects = GetGame().GetObjectManager();
			const auto target = objects.Get(agent);
			const auto source = objects.Get(initiator);

			ModifierDurationRules rules;
			rules.isBuff = settings["isBuff"];
			rules.isDebuff = settings["isDebuff"];
			rules.isDoT = settings["isDoT"];
			rules.isCrowdControl = settings["isCrowdControl"];
			rules.buffIncrease = source ? source->GetAttributeValue(AttributeType::BuffDuration) : 0.f;
			rules.debuffReduction = target ? target->GetAttributeValue(AttributeType::DebuffDuration) : 0.f;
			rules.dotReduction = target ? target->GetAttributeValue(AttributeType::DoTDurationDecrease) : 0.f;
			rules.crowdControlReduction = target ? target->GetAttributeValue(AttributeType::CrowdControlDurationDecrease) : 0.f;

			return GetGame().GetModifiers().Request(spec, rules, settings.get<float>("seconds"), agent, initiator, std::move(context));
		};
		modifierEngine["GetContext"] = [this](sol::this_state L, uint32_t id) {
			return GetGame().GetModifiers().GetContext(id, L);
		};
		modifierEngine["GetStackCount"] = [this](uint32_t id) {
			const auto instance = GetGame().GetModifiers().GetManager().Get(id);
			return instance ? instance->stackCount : 0;
		};
		modifierEngine["SetStackCount"] = [this](uint32_t id, uint32_t count) {
			GetGame().GetModifiers().SetStackCount(id, count);
		};
		modifierEngine["ResetDuration"] = [this](uint32_t id) {
			GetGame().GetModifiers().ResetDuration(id);
		};
		modifierEngine["GetRemainingSeconds"] = [this](uint32_t id) {
			return GetGame().GetModifiers().GetManager().GetRemainingMs(id, GetGame().GetTime()) / 1000.0;
		};
		modifierEngine["MarkForDelete"] = [this](uint32_t id) {
			GetGame().GetModifiers().MarkForDelete(id);
		};
		modifierEngine["GetFirst"] = [this](uint32_t agent, uint32_t guid) -> uint32_t {
			const auto instance = GetGame().GetModifiers().GetManager().FindFirst(agent, guid);
			return instance ? instance->id : 0;
		};
		modifierEngine["GetAll"] = [this](uint32_t agent) {
			return sol::as_table(GetGame().GetModifiers().GetManager().GetAll(agent));
		};
		modifierEngine["AddAttributeChange"] = [this](uint32_t object, uint8_t attribute, float value, uint8_t op, uint32_t id) {
			return GetGame().GetModifiers().AddAttributeChange(object, AttributeFromScript(attribute), value, static_cast<AttributeOp>(op), id);
		};
		modifierEngine["RemoveAttributeChange"] = [this](uint32_t object, uint32_t handle, uint32_t id) {
			GetGame().GetModifiers().RemoveAttributeChange(object, handle, id);
		};

		LoadBuffer(setupApi);
		LoadBuffer(castApi);
		LoadBuffer(objectApi);
		LoadBuffer(modifierApi);
		LoadBuffer(vectorApi);
		LoadBuffer(watchApi);
	}
}
