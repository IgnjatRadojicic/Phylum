
// Include
#include "difficultytuning.h"
#include "magicnumbers.h"
#include "instance.h"
#include "lua.h"
#include "modifiersystem.h"
#include "object.h"
#include "objectmanager.h"
#include "scriptgaps.h"
#include "scriptsource.h"

#include "utils/functions.h"

#include <format>
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
			-- Many files only exist under the hash of their name, and some sit in lua/ instead of the
			-- folder they are asked by (Objectives!objective_LootCrystals.lua).
			local path
			for _, candidate in ipairs({ directory .. "/" .. file, directory .. "/" .. HashedScriptName(file),
				"lua/" .. file, "lua/" .. HashedScriptName(file) }) do
				local handle = io.open("data/serverdata/" .. candidate .. ".lua")
				if handle then
					handle:close()
					path = "data/serverdata/" .. candidate .. ".lua"
					break
				end
			end
			local source = path and ReadGameScript(path)
			if not source then
				-- The game asks for nine files it never shipped (and one misspelt folder, "Modifers!"),
				-- yet those scripts work in it, so a missing one loads as nothing instead of failing.
				if name:match("^[^!]+!.+%.lua$") then
					if ScriptGaps then
						ScriptGaps.Record("require." .. name)
					end
					return function() end
				end
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

		-- A thread started by an ability belongs to the same cast. One started by a
		-- modifier also ends with it (modifier_cryosboss_passive's spawner waits forever).
		function nThread.CreateThreadForObject(id, callback, ...)
			local cast = currentCast()
			nThread.Create(id, function(...)
				casts[running()] = cast
				if cast and cast.modifierId then
					ModifierEngine.AdoptThread(cast.modifierId)
				end
				return callback(...)
			end, ...)
		end

		-- Scripts index their rank tables from 1 (energyDefensePercent[rank]), and a squad ability
		-- comes with rank 0, so anything below 1 is rank 1.
		function ScriptRankOf(rank)
			rank = tonumber(rank)
			return (rank and rank >= 1) and rank or 1
		end

		function nAbility.BeginCast(ability, object, target, cursorPosition, rank)
			nextInstanceId = nextInstanceId + 1
			local id = object:GetId()
			-- Where the caster stands decides where shots start. Never lets the trace break a cast.
			local ok, at = pcall(function()
				local p = object:GetPosition()
				local targetId = target and target:GetId() or 0
				return string.format(" at=%.1f,%.1f,%.1f target=%d", p.x, p.y, p.z, targetId)
			end)
			ScriptTrace("CAST ability=" .. tostring(ability.id) .. " object=" .. tostring(id) .. (ok and at or ""))
			-- Each ability walks its own animation sequence: one object's abilities have sequences
			-- of different lengths, so a shared step would run past a shorter one.
			local steps = ability.animationSequence and #ability.animationSequence or 0
			local objectSteps = sequenceSteps[id] or {}
			sequenceSteps[id] = objectSteps
			local step = 0
			if steps > 0 then
				step = (objectSteps[ability] or 0) % steps
				objectSteps[ability] = (step + 1) % steps
			end
			casts[running()] = {
				ability = ability,
				object = object,
				target = target,
				cursorPosition = cursorPosition,
				rank = ScriptRankOf(rank),
				sequenceIndex = step,
				instanceId = nextInstanceId,
				startTime = nGameSimulator.GetGameTime()
			}
		end

		-- Seconds since the running cast or modifier began. Its threads share it, so a
		-- handler can set a deadline that the modifier's own loop then checks.
		function nThread.GetDurationElapsed()
			local cast = currentCast()
			if cast and cast.startTime then
				return nGameSimulator.GetGameTime() - cast.startTime
			end
			return 0
		end

		-- The server starts a cast through ability.castTick, which sets up the cast
		-- and then runs the script's tick. It is kept apart from ability.tick because
		-- scripts call self:tick() themselves: many numbered ticks are just
		-- "return self:tick()", handing over to their template's tick, and would
		-- call themselves forever if ability.tick were replaced by the wrapper.
		local function castingTick(tick)
			return function(self, object, target, cursorPosition, rank)
				if object then
					nAbility.BeginCast(self, object, target, cursorPosition, rank)
					WatchCastEnd(currentCast())
					-- The cast's Activate step runs first, in the same cast: scripts keep what their
					-- tick needs there (soulravager_active counts its shots). As modifiers do, a
					-- numbered one wins over the named one, and hasActivate = false turns it off.
					local activate = self.hasActivate ~= false and (self[nAbilityFns.Activate] or self.activate)
					if activate then
						activate(self)
					end
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
				ability.castTick = castingTick(tick)
			end
			registerAbility(name, ability)
		end

		-- Runs however a cast ended, so what it started comes off even if its script failed
		function nAbility.EndCast(context)
			CastContext.Set(context)
			local ability = context.ability
			local deactivate = ability[nAbilityFns.Deactivate] or ability.deactivate
			if deactivate then
				deactivate(ability)
			end
		end

		-- Some scripts set tick by its nAbilityFns number, after they register
		function nAbility.NumberedTick(ability)
			local tick = ability[nAbilityFns.Tick]
			if tick then
				ability.castTick = castingTick(tick)
				return ability.castTick
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

		-- A script makes an object cast another ability: a summon, a charge's hit, a pet's attack.
		-- It starts at once in its own cast, and skips the cost and cooldown of a cast the player asks for.
		function nAbility.RequestAbility(ability, agentId, targetId, x, y, z, rank)
			local agent = agentId and agentId ~= kObjIDNone and nObjectManager.GetObject(agentId) or nil
			if not agent then
				return
			end
			-- false, not nil: the engine side cannot take nil for an object it may not get.
			local target = targetId and targetId ~= kObjIDNone and nObjectManager.GetObject(targetId) or false
			agent:UseAbility(ability, target, vec3.new(x or 0, y or 0, z or 0), rank)
		end

		-- What a cast costs, as the ability's script gives it. AbilityCost (abilitycost.h) adds the
		-- caster's part. The game's own nAbility.GetManaCost (global.lua) finds the ability by its
		-- namespace, so it is used when that name leads back to this ability.
		AbilityApi = {}
		function AbilityApi.CostData(ability, agentId, rank)
			local manaCost
			if ability.namespace and rawget(_G, ability.namespace) == ability then
				manaCost = nAbility.GetManaCost(ability.namespace, agentId, rank)
			else
				manaCost = GetRankedValueHelper(ability.manaCost or 0, rank)
			end
			local manual = nCooldownType ~= nil and ability.cooldownType ~= nil and ability.cooldownType == nCooldownType.Manual
			return manaCost or 0,
				GetRankedValueHelper(ability.manaCoefficient or 0, rank) or 0,
				GetRankedValueHelper(ability.cooldown or 0, rank) or 0,
				manual,
				GetRankedValueHelper(ability.cooldownVariance or 0, rank) or 0
		end

		function AbilityApi.Range(ability, rank)
			local range = GetRankedValueHelper(ability.range or 0, rank)
			return type(range) == "number" and range or 0
		end

		function nAbility.GetAbilityAttributeValue(agentId, abilityId)
			local agent = agentId and agentId ~= kObjIDNone and nObjectManager.GetObject(agentId) or nil
			return agent and agent:GetAbilityAttributeValue(abilityId) or 0
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

		-- The server calls this when a thread goes back to its pool. The thread is used
		-- again for other work, which must not find this one's cast or private data.
		function CastContext.Forget(thread)
			casts[thread] = nil
			looseData[thread] = nil
		end

		-- A name gives a separate table. The engine fills "RequestModifierParams" with the table a
		-- modifier was requested with (RequestModifier's sixth argument: the knockback's direction).
		local function namedTables(owner)
			owner.namedTables = owner.namedTables or {}
			return owner.namedTables
		end

		function nThreadData.CreatePrivateTable(name)
			local owner = dataOwner()
			if name ~= nil then
				namedTables(owner)[name] = {}
				return owner.namedTables[name]
			end
			owner.privateTable = {}
			return owner.privateTable
		end

		function nThreadData.GetPrivateTable(name)
			local owner = dataOwner()
			if name ~= nil then
				local tables = namedTables(owner)
				if tables[name] == nil and name == "RequestModifierParams" then
					local params = owner.properties and owner.properties[1]
					tables[name] = type(params) == "table" and params or nil
				end
				tables[name] = tables[name] or {}
				return tables[name]
			end
			owner.privateTable = owner.privateTable or {}
			return owner.privateTable
		end

		local function setData(key, value)
			nThreadData.GetPrivateTable()[key] = value
		end

		-- In a modifier, a number nobody set is the value its RequestModifier passed, numbered from 0
		-- like GetFloatProperty (Shadow Ravager's vulnerability reads the hit's damage as GetFloat(0)).
		local function getData(key)
			local value = nThreadData.GetPrivateTable()[key]
			if value == nil and type(key) == "number" then
				local properties = dataOwner().properties
				value = properties and properties[key + 1]
			end
			return value
		end

		nThreadData.SetInt, nThreadData.GetInt = setData, getData
		nThreadData.SetFloat, nThreadData.GetFloat = setData, getData
		nThreadData.SetGUID, nThreadData.GetGUID = setData, getData

		function nDebug.IsAbilityDebugEnabled()
			return false
		end

		-- A manual ability (cooldownType Manual) pays for its cast when its script says so. Every
		-- other ability paid when the cast began, so its call does nothing: a cast pays once. The
		-- lightning and missile tempests and the sleep mushroom call it without being manual.
		function nAbility.PayCooldownAndMana()
			local cast = currentCast()
			if not (cast and cast.object and cast.ability and cast.ability.id) or cast.paid then
				return
			end
			local ability = cast.ability
			if nCooldownType and ability.cooldownType == nCooldownType.Manual then
				cast.paid = true
				cast.object:PayForCast(ability.id, cast.rank)
			end
		end

		-- Cooldown changes, as the client's own nAbility functions make them (see CooldownTable).
		-- An ability is an id, or a name as the client also takes it.
		local function cooldownAbility(ability)
			if type(ability) == "string" then
				return nUtil.SPID(ability)
			end
			return tonumber(ability)
		end

		local function cooldownAgent(agentId)
			return agentId and agentId ~= kObjIDNone and nObjectManager.GetObject(agentId) or nil
		end

		function nAbility.ResetAbilityCooldown(agentId, ability, targetId, rank)
			local agent, id = cooldownAgent(agentId), cooldownAbility(ability)
			if agent and id then
				agent:RestartCooldown(id, tonumber(rank) or 1)
			end
		end

		function nAbility.AddCooldownTime(agentId, ability, seconds)
			local agent, id = cooldownAgent(agentId), cooldownAbility(ability)
			if agent and id then
				agent:ShiftCooldown(id, (tonumber(seconds) or 0) * 1000)
			end
		end

		function nAbility.RemoveCooldownTime(agentId, ability)
			local agent = cooldownAgent(agentId)
			if agent then
				agent:ClearCooldown(cooldownAbility(ability))
			end
		end

		function nAbility.ScaleCooldownTime(agentId, scale, ability)
			local agent = cooldownAgent(agentId)
			if agent then
				agent:ScaleCooldown(tonumber(scale) or 1, cooldownAbility(ability))
			end
		end

		-- Scripts read this back with GetAbilityEventType. Events do not reach anything yet.
		function nAbility.CreateAbilityEvent(eventType)
			return { eventType = eventType }
		end
		function nAbility.SendAbilityEvent() end

		-- A cast or modifier can take control of its agent, as a resurrect does while it channels
		-- (Darkspore.exe 0xa65320, 0xa65360). The client reads no argument and binds the agent of the
		-- cast that calls it. Scripts ask IsControlledByAbility, so a pet leaves its owner alone then.
		-- Approximate: the server does not stop the agent's own actions while it is bound.
		local boundBy = {}
		local function agentOf(cast)
			return cast and cast.object and cast.object:GetId() or nil
		end

		function nAbility.BindAgent(_)
			local cast = currentCast()
			local id = agentOf(cast)
			if id then
				boundBy[id] = cast
			end
		end

		function nAbility.ReleaseAgent(_)
			local cast = currentCast()
			local id = agentOf(cast)
			if id and boundBy[id] == cast then
				boundBy[id] = nil
			end
		end

		function nAbility.IsControlledByAbility(id)
			local cast = id and boundBy[id]
			if not cast then
				return false
			end
			-- Ids are used again, so the bound cast must still belong to the same object.
			if cast.object ~= nObjectManager.GetObject(id) then
				boundBy[id] = nil
				return false
			end
			return true
		end

		-- Whether a cast with this instance id still runs (global.lua's debug drawing waits on it).
		function nAbility.IsAbilityRunning(instanceId)
			for _, cast in pairs(casts) do
				if type(cast) == "table" and cast.instanceId == instanceId then
					return true
				end
			end
			return false
		end

		-- An ability's script by its id, which is nUtil.SPID of the name it registered with.
		local function abilityScript(ability)
			if type(ability) == "table" then
				return ability
			end
			for name, script in pairs(nAbility.scripts) do
				if nUtil.SPID(name) == ability then
					return script
				end
			end
			return nil
		end

		function nAbility.GetAbilityNamespace(ability)
			local script = abilityScript(ability)
			return script and script.namespace or nil
		end

		-- The descriptor bits the client keeps for an ability (0xa65a50).
		function nAbility.GetDescriptors(ability)
			local script = abilityScript(ability)
			return script and script.descriptors or 0
		end

		-- A property of the cast's ability by name. The client looks it up in the ability's
		-- definition (0xa676b0); the server has the script's fields.
		function nAbility.GetProperty(name)
			local cast = currentCast()
			local ability = cast and cast.ability
			return ability and ability[name] or nil
		end

		-- The client takes the agent's attributes again for the cast. The server reads them live
		-- each time (GetAgentAttributeSnapshot), so there is nothing to take again.
		function nAbility.RetakeAgentSnapshot() end
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

		-- GetIsTargetable(id): the client asks the object (0xa03320), false when there is none.
		function nGameObject.GetIsTargetable(id)
			local object = get(id)
			return object ~= nil and object:IsTargetable()
		end

		-- AddDependency(a, b): the client sets one flag (+0x98) on both objects (0xa03e60). Nothing on
		-- the server reads it yet, so it is left out until that use is known.
		function nGameObject.AddDependency() end

		-- Aggro (taunts, turrets drawing enemies, a pet's attacker passed to its owner) needs enemy AI
		-- that picks targets by it, which is Stage 2. Until then nothing is passed on.
		function nGameObject.AddAggroForObject() end
		function nGameObject.SetAggroDelegate() end
		function nGameObject.GetAggroDelegate()
			return kObjIDNone
		end

		-- GetObjectAngleRad(id, otherId): the turn from the object's facing to the other object, about
		-- the up axis, from 0 to 2 pi (client 0xa0da40). Nothing when either one is missing, as there.
		function nGameObject.GetObjectAngleRad(id, otherId)
			local object, other = get(id), get(otherId)
			if not object or not other then
				return
			end

			local from, to = object:GetPosition(), other:GetPosition()
			local fx, fy = nGameObject.GetFacing(id)
			if fx * fx + fy * fy < 0.000001 then
				fx, fy = 0, 1
			end
			local angle = math.atan2(to.y - from.y, to.x - from.x) - math.atan2(fy, fx)
			return angle % (2 * math.pi)
		end

		function nGameObject.GetCenterPoint(id)
			local object = get(id)
			if object then
				return unpackVector(object:GetCenterPoint())
			end
			return 0, 0, 0
		end

		-- The server keeps no terrain, so where an object stands is already on the ground.
		nGameObject.GetGroundPosition = nGameObject.GetPosition

		local function forward(method, fallback)
			return function(id, ...)
				local object = get(id)
				if object then
					return object[method](object, ...)
				end
				return fallback
			end
		end

		nGameObject.GetManaPoints = forward("GetMana", 0)
		nGameObject.GetMaxManaPoints = forward("GetMaxMana", 0)
		nGameObject.SetManaPoints = forward("SetMana")
		nGameObject.GetIsStealthed = forward("IsStealthed", false)
		nGameObject.KillObject = forward("Kill")

		-- The client keeps one flag for it and reads the value as Lua truth.
		function nGameObject.SetIsVisible(id, visible)
			local object = id and id ~= kObjIDNone and nObjectManager.GetObject(id) or nil
			if object then
				object:SetVisible(visible and true or false)
			end
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

		function nGameObject.GetMaxHitPoints(id)
			local object = get(id)
			return object and object:GetMaxHealth() or 0
		end

		-- Edge to edge, as the client's (0xa0b5c0): the centres' distance less both footprints, never below 0.
		function nGameObject.GetObjectDistance(firstId, secondId)
			local first, second = get(firstId), get(secondId)
			if not first or not second then
				return 0
			end
			local from, to = first:GetPosition(), second:GetPosition()
			local x, y, z = to.x - from.x, to.y - from.y, to.z - from.z
			local gap = math.sqrt(x * x + y * y + z * z) - first:GetFootprintRadius() - second:GetFootprintRadius()
			return math.max(gap, 0)
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

		-- The client has no IsPet; the original server's is not known. A pet here is a creature that
		-- another creature owns, such as a summon: the templates then add the owner's PetDamage.
		-- A projectile has an owner too, but is no creature.
		function nGameObject.IsPet(id)
			local object = get(id)
			local owner = object and object:GetOwnerObject()
			return owner ~= nil and owner ~= object and object:IsCreature() and owner:IsCreature()
		end

		function nGameObject.IsAlive(id)
			local object = get(id)
			return object ~= nil and object:GetHealth() > 0
		end

		-- Scripts pass the caster's team, not the caster (nGameObject.GetTeam(agent), or a stored myTeam).
		function nGameObject.ValidateHostileTarget(team, targetId)
			local target = get(targetId)
			return target ~= nil and target:GetHealth() > 0 and target:GetTeam() ~= team
		end

		function nGameObject.ValidateFriendlyTarget(team, targetId)
			local target = get(targetId)
			return target ~= nil and target:GetHealth() > 0 and target:GetTeam() == team
		end

		function nGameObject.TakeDamage(snapshot, targetId, damage, damageType, damageSource, coefficient, descriptors, multiplier, x, y, z)
			local target = get(targetId)
			if not target then
				return false, 0, false
			end
			local direction = x and vec3.new(x, y, z) or nil
			-- With no caster there is no attacker (a hazard), so the victim gets no DealtDamage event.
			local attacker = nAbility.GetCaster()
			return target:TakeDamage(attacker, snapshot or false, damage, damageType or 0, damageSource or 0, coefficient or 0, descriptors or 0, multiplier, direction)
		end

		-- An amount is one number, or a table with the lowest and highest
		function nGameObject.HealDamage(snapshot, targetId, amount, coefficient, descriptors, critical, canResurrect)
			local target = get(targetId)
			if not target then
				return 0, false
			end
			local range = type(amount) == "table" and amount or { amount or 0, amount or 0 }
			local healer = nAbility.GetCaster() or target
			return target:Heal(healer, snapshot, range, coefficient or 0, descriptors or 0, critical == true, canResurrect == true)
		end

		nAttribute = nAttribute or {}
		function nAttribute.GetAttributeValue(id, attribute)
			local object = get(id)
			return object and object:GetAttributeValue(ModifierEngine.AttributeFromScript(attribute)) or 0
		end

		-- A value from an attribute snapshot (such as a modifier's initiator), as from a live object.
		function nAttribute.GetAttributeValue_FromSnapshot(snapshot, attribute)
			if type(snapshot) ~= "userdata" then
				return 0
			end
			return snapshot:GetValue(ModifierEngine.AttributeFromScript(attribute)) or 0
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
			context.startTime = nGameSimulator.GetGameTime()
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

		-- The event reads and writes as the modifier too, because handlers use their one argument as both
		function ModifierApi.Stack(context)
			CastContext.Set(context)
			local modifier = context.ability
			local handler = scriptFunction(modifier, nAbilityFns.HandleEvent, "handleEvent", "hasHandleEvent")
			if handler then
				local event = setmetatable({ eventType = nAbilityEventFlags.StackModifier }, { __index = modifier, __newindex = modifier })
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

		-- An event from the server (see AbilityEvent in abilityevent.h), for a script that lists
		-- its type in handledEvents. As with stacking, the event reads as the modifier too.
		function ModifierApi.HandleEvent(context, data)
			CastContext.Set(context)
			local modifier = context.ability
			if not hasFlag(modifier.handledEvents, data.eventType) then
				return
			end
			local handler = scriptFunction(modifier, nAbilityFns.HandleEvent, "handleEvent", "hasHandleEvent")
			if handler then
				ScriptTrace("EVENT type=" .. data.eventType .. " modifier=" .. tostring(context.modifierId) .. " from=" .. tostring(data.guids[1]))
				local event = setmetatable({ eventType = data.eventType, guids = data.guids, floats = data.floats, ints = data.ints },
					{ __index = modifier, __newindex = modifier })
				context.handled = handler(event) == true
			end
		end

		-- A class's always-on modifier (see PassiveModifiers in passivemodifiers.h). Some class
		-- data names something that is no modifier, which is left alone.
		function ModifierApi.StartPassive(agent, guid)
			local skip = not nModifier.registered[guid] and "unregistered"
				or ModifierEngine.GetFirst(agent, guid) ~= 0 and "running"
			if skip then
				ScriptTrace("PASSIVE agent=" .. agent .. " guid=" .. guid .. " skipped=" .. skip)
				return 0
			end
			-- A request keeps its cast context per coroutine, so it needs one. A plain one,
			-- not the object's own thread, so nothing that cancels the object's cast reaches it.
			local request = coroutine.create(function()
				return nModifier.RequestModifier(agent, agent, guid, nil, 1)
			end)
			local ok, id = coroutine.resume(request)
			CastContext.Forget(request)
			if not ok then
				error(id)
			end
			id = (id and id ~= kObjIDNone) and id or 0
			ScriptTrace("PASSIVE agent=" .. agent .. " guid=" .. guid .. " id=" .. id)
			return id
		end

		function nAbility.CheckDescriptors_AnyMatch(descriptors, wanted)
			return nBit.And(descriptors or 0, wanted or 0) ~= 0
		end

		function nAbility.GetAbilityEventGUIDData(event, slot)
			return event and event.guids and event.guids[slot] or 0
		end

		function nAbility.GetAbilityEventFloatData(event, slot)
			return event and event.floats and event.floats[slot] or 0
		end

		function nAbility.GetAbilityEventIntData(event, slot)
			return event and event.ints and event.ints[slot] or 0
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

			rank = ScriptRankOf(rank)

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

		-- Without an operation a change adds. Inside a modifier it ends with the modifier. The client
		-- reads the value with lua_tonumber (0xa04000), so one that is no number adds 0
		-- (FireRavagerEnergyVulnerability passes its ranked table).
		function nAttribute.AddAttributeModifier(id, attribute, value, operation)
			return ModifierEngine.AddAttributeChange(id, attribute, tonumber(value) or 0, operation or nAttributeOp.kAdd, instanceOf(nil))
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
			-- The client reads the radius with lua_tonumber (0xa10260), so one that is no number is 0
			-- (BabyToss passes its radius table).
			nObjectManager[name] = function(x, y, z, radius, types)
				return toIds(query(vector(x, y, z), tonumber(radius) or 0, type(types) == "table" and types or {}))
			end
		end

		function nObjectManager.GetObjectsInRadius_SortedByFurthestDistance(x, y, z, radius, types)
			local nearest = nObjectManager.GetObjectsInRadius_SortedByDistance(x, y, z, radius, types)
			local furthest = {}
			for index = #nearest, 1, -1 do
				furthest[#furthest + 1] = nearest[index]
			end
			return furthest
		end

		-- TargetUtils passes its type list where the radius goes. Read as a number that is 0,
		-- leaving no type list, which is what the game's own engine made of it.
		local alongLine = nObjectManager.GetObjectsAlongLine
		function nObjectManager.GetObjectsAlongLine(x1, y1, z1, x2, y2, z2, radius, types)
			radius = tonumber(radius) or 0
			return toIds(alongLine(vector(x1, y1, z1), vector(x2, y2, z2), radius, type(types) == "table" and types or {}))
		end

		-- CreateCreature(noun, creatureId, x, y, z, [value, dx, dy, dz, extra]). The noun is a name or
		-- the id nUtil.GetAsset gives. The client reads the creature id as a 64-bit number, uses the
		-- value only when it is not 0, and the extra only for a player's creature; none is used here yet.
		local createCreature = nObjectManager.CreateCreature
		function nObjectManager.CreateCreature(noun, creatureId, x, y, z, value, dx, dy, dz, extra)
			local nounId = type(noun) == "string" and nUtil.SPID(noun) or tonumber(noun)
			if not nounId then
				return kObjIDNone
			end
			local direction = dx and vector(dx, dy, dz) or nil
			local id = createCreature(nounId, vector(x, y, z), direction)
			return id ~= 0 and id or kObjIDNone
		end

		-- Without a navigation mesh there is no better spot to offer, so the asked one is given back.
		function nLocomotion.GetClosestSpawnPosition(noun, x, y, z)
			return x or 0, y or 0, z or 0
		end

		-- Trigger volumes (ScriptTriggerVolumes). The client calls a volume's enter, exit and stay
		-- functions with its handle and the object's id, in the script that made it, so they can
		-- still ask about their own cast. A missing function goes to the engine as false.
		local function inMakersCast(callback)
			if type(callback) ~= "function" then
				return false
			end
			local cast = CastContext.Get()
			return function(...)
				CastContext.Set(cast)
				return callback(...)
			end
		end

		local function volumeOrNil(volume)
			return volume ~= 0 and volume or nil
		end

		local attachTriggerVolume = nObjectManager.AttachTriggerVolume
		function nObjectManager.AttachTriggerVolume(id, radius, enter, exit, stay)
			if not id or id == kObjIDNone then
				return nil
			end
			return volumeOrNil(attachTriggerVolume(id, tonumber(radius) or 0,
				inMakersCast(enter), inMakersCast(exit), inMakersCast(stay)))
		end

		local createTriggerVolume = nObjectManager.CreateTriggerVolume
		function nObjectManager.CreateTriggerVolume(x, y, z, radius, enter, exit, stay)
			return volumeOrNil(createTriggerVolume(vector(x or 0, y or 0, z or 0), tonumber(radius) or 0,
				inMakersCast(enter), inMakersCast(exit), inMakersCast(stay)))
		end

		-- (x, y, z, width, length, height, facing x, y, z, enter, exit, stay). The firewall gives
		-- its scale as the length.
		local createTriggerVolumeBox = nObjectManager.CreateTriggerVolumeBox
		function nObjectManager.CreateTriggerVolumeBox(x, y, z, width, length, height, fx, fy, fz, enter, exit, stay)
			return volumeOrNil(createTriggerVolumeBox(vector(x or 0, y or 0, z or 0),
				vector(width or 0, length or 0, height or 0), vector(fx or 0, fy or 0, fz or 0),
				inMakersCast(enter), inMakersCast(exit), inMakersCast(stay)))
		end

		local destroyTriggerVolume = nObjectManager.DestroyTriggerVolume
		function nObjectManager.DestroyTriggerVolume(volume)
			if volume then
				destroyTriggerVolume(volume)
			end
		end

		-- The client ends the volume here as DestroyTriggerVolume does (Darkspore.exe 0xa00930).
		function nObjectManager.DetachTriggerVolume(_, volume)
			nObjectManager.DestroyTriggerVolume(volume)
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
		-- Scripts give no height, and the engine function reads the radius from the third number
		function nMathUtil.CircleIntersectsArc(x, y, radius, ...)
			return circleIntersectsArc(vector(x, y, radius), radius, ...)
		end

		-- Approximate: waits it out, and the one script using this checks the target after each wait
		function nThread.WaitForNotValidTarget(seconds)
			nThread.WaitForXSeconds(seconds)
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

		-- WaitForControlOfAgent(agent, timeout): whether this script now controls the agent (fear runs the
		-- agent to random spots while it does). Control is not shared out on the server yet, so a living
		-- agent is always controlled. One update passes first, so a loop on it never stalls the game.
		function nThread.WaitForControlOfAgent(agentId, timeout)
			nThread.Sleep(0)
			local agent = tonumber(agentId) and nObjectManager.GetObject(agentId) or nil
			return agent ~= nil and agent:GetHealth() > 0
		end

		-- A delay can come as a table holding the number (Psistorm sets timetohit = { timeToHit }).
		for _, name in ipairs({ "WaitForXSeconds", "WaitUntilTime" }) do
			local wait = nThread[name]
			nThread[name] = function(seconds, ...)
				if type(seconds) == "table" then
					seconds = seconds[1]
				end
				return wait(tonumber(seconds) or 0, ...)
			end
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

		-- Until the object's jump or slide lands. The client plays it from ObjectJump (JumpArc).
		function nThread.WaitForJumpComplete(id)
			local object = get(id)
			while object and object:IsJumping() do
				nThread.WaitForXSeconds(0.05)
			end
		end

		-- WaitForNearGoal(id, [distance, fraction, timeout, giveUpIfNotCloser]): until the object's walk
		-- comes within distance of its goal (true), or the object is gone or the timeout runs out
		-- (false). The server places the object at once, so it waits on what the client still walks.
		-- The fraction and the give-up flag are left out: a counted walk always gets closer.
		function nThread.WaitForNearGoal(id, distance, fraction, timeout, giveUpIfNotCloser)
			distance = math.max(tonumber(distance) or 0, 0)
			timeout = tonumber(timeout) or -1
			while true do
				local object = get(id)
				if not object then
					return false
				end
				if object:GetWalkDistanceLeft() <= distance then
					return true
				end
				if timeout > 0 then
					timeout = timeout - 0.05
					if timeout <= 0 then
						return false
					end
				end
				nThread.WaitForXSeconds(0.05)
			end
		end

		function nGameObject.GetModifiedMoveSpeed(id)
			local object = get(id)
			return object and object:GetModifiedMovementSpeed() or 0
		end

		-- GetRecentDamage(id, cap, seconds, [includeAll]): what the object took in the last seconds,
		-- at most cap (RecentDamage).
		function nGameObject.GetRecentDamage(id, cap, seconds, includeAll)
			if not get(id) then
				return 0
			end
			return Game.GetRecentDamage(id, tonumber(cap) or 0, tonumber(seconds) or 0)
		end

		-- The server has no navigation mesh yet, so there is nothing to walk through.
		function nGameObject.SetNavCollision(id, collides)
		end

		-- The server has no physics bodies either. Creatures have no moving body in the client too,
		-- so IsDynamic is false and scripts skip the push they give loose props.
		nPhysics = nPhysics or {}
		function nPhysics.IsDynamic(id)
			return false
		end
		function nPhysics.AddPhysicsForObject(id)
		end
		function nPhysics.RemovePhysicsForObject(id)
		end
		function nPhysics.SetObjectAsCollidable(id, collidable)
		end

		-- The server keeps no terrain, so a point is already on the ground.
		function nLocomotion.GetTerrainHeight(x, y, z)
			return tonumber(z) or 0
		end

		-- FindGoodMeleePosition(agent, target): found, x, y, z. The spot touching the target on the
		-- agent's side. Approximate: the client picks it on its navigation mesh, away from others.
		function nLocomotion.FindGoodMeleePosition(agentId, targetId)
			local agent, target = get(agentId), get(targetId)
			if not agent or not target then
				return false, 0, 0, 0
			end

			local from, to = agent:GetPosition(), target:GetPosition()
			local dx, dy = from.x - to.x, from.y - to.y
			local length = math.sqrt(dx * dx + dy * dy)
			if length < 0.001 then
				local facing = target:GetFacing()
				dx, dy, length = facing.x, facing.y, math.sqrt(facing.x * facing.x + facing.y * facing.y)
				if length < 0.001 then
					dx, dy, length = 0, 1, 1
				end
			end

			local reach = agent:GetBodyRadius() + target:GetBodyRadius()
			return true, to.x + dx / length * reach, to.y + dy / length * reach, to.z
		end

		-- Whether an agent can get to a point (Chrono Blink, Blitz's strike, fleeing). Approximate:
		-- the server has no navigation mesh yet, so every point counts as reachable.
		function nLocomotion.IsPositionReachable(agentId, x, y, z)
			return get(agentId) ~= nil
		end
		nLocomotion.IsPositionReachableExact = nLocomotion.IsPositionReachable

		-- FindBallRollPosition(agent, x, y, z): found, x, y, z. The client rolls along its navigation
		-- mesh toward the point to the last spot it reaches; with no mesh that is the point itself.
		function nLocomotion.FindBallRollPosition(agentId, x, y, z)
			if not get(agentId) then
				return false, 0, 0, 0
			end
			return true, tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0
		end

		-- The closest walkable point to a point, as three numbers (global.lua's random point, the
		-- Lightning Tempest, charges, flee). With no navigation mesh that is the point itself.
		function nLocomotion.GetClosestPositionFromPoint(x, y, z)
			return tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0
		end

		function nLocomotion.GetClosestPosition(agentId, x, y, z)
			return nLocomotion.GetClosestPositionFromPoint(x, y, z)
		end
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
		if (mTraceObserver) {
			mTraceObserver(line);
		}
	}

	void Lua::SetTraceObserver(std::function<void(const std::string&)> observer) {
		mTraceObserver = std::move(observer);
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

		// Scripts run both in the client and here, and take the server's branch on false.
		sol::table scenarioManager = mState.create_named_table("nScenarioManager");
		scenarioManager["GetIsClient"] = [] {
			return false;
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

		// In C++ because Lua's own formatting cannot hold a hash this large on Windows.
		mState["HashedScriptName"] = [](const std::string& name) {
			return std::format("0x{:08X}", utils::hash_id(name));
		};

		auto gapLog = std::make_shared<std::ofstream>(mGapLogPath, std::ios::app);
		auto gaps = std::make_shared<ScriptGaps>(*gapLog);
		sol::table scriptGaps = mState.create_named_table("ScriptGaps");
		scriptGaps["Record"] = [gapLog, gaps](const std::string& name) {
			gaps->Record(name);
		};

		mState["WatchCastEnd"] = [this](sol::this_state L, sol::table context) {
			if (auto thread = GetThread(L)) {
				thread->mCastContext = sol::main_table(context);
			}
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
		// Scripts turn percentages into ratings with this, so it follows the level being played.
		sol::table tuning = mState["nTuning"];
		tuning["DefenseRatingPerPercent"] = [this]() {
			return DifficultyTuning::Game().GetRatingConversion(GetGame().GetChainData().GetLevelIndex());
		};
		tuning["DodgePercentCap"] = [] { return MagicNumbers::Game().dodgePercentCap; };
		tuning["ResistPercentCap"] = [] { return MagicNumbers::Game().resistPercentCap; };

		modifierEngine["AdoptThread"] = [this](sol::this_state L, uint32_t id) {
			const auto thread = GetThread(L);
			return thread && GetGame().GetModifiers().AdoptThread(id, TrackThread(thread));
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
