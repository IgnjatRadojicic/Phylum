
// Include
#include "instance.h"
#include "lua.h"
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

		-- Modifiers register but have no effect yet
		nModifier = nModifier or {}
		nModifier.registered = {}
		function nModifier.RegisterModifier(name, modifier)
			nModifier.registered[name] = modifier
		end
		function nModifier.PreloadAsset(name)
			return nUtil.GetAsset(name)
		end
		function nModifier.RequestModifier() end
		function nModifier.CheckForModifierDebuffDescriptor()
			return false
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

		local function currentCast()
			return casts[running()]
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

		nAbility.scripts = {}
		local registerAbility = nAbility.RegisterAbility
		function nAbility.RegisterAbility(name, ability)
			nAbility.scripts[name] = ability
			local tick = ability.tick
			if tick then
				ability.tick = function(self, object, target, cursorPosition, rank)
					nAbility.BeginCast(self, object, target, cursorPosition, rank)
					return tick(self)
				end
			end
			registerAbility(name, ability)
		end

		function nAbility.GetAgentID()
			return currentCast().object:GetId()
		end

		-- Nil outside an ability, for example while a modifier runs
		function nAbility.GetCaster()
			local cast = currentCast()
			return cast and cast.object or nil
		end

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
			return object and object:GetAttributeValue(attribute) or 0
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
		simulator["IsChainGame"] = [] {
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

		auto gapLog = std::make_shared<std::ofstream>(mGapLogPath, std::ios::app);
		auto gaps = std::make_shared<ScriptGaps>(*gapLog);
		sol::table scriptGaps = mState.create_named_table("ScriptGaps");
		scriptGaps["Record"] = [gapLog, gaps](const std::string& name) {
			gaps->Record(name);
		};

		LoadBuffer(setupApi);
		LoadBuffer(castApi);
		LoadBuffer(objectApi);
		LoadBuffer(vectorApi);
		LoadBuffer(watchApi);
	}
}
