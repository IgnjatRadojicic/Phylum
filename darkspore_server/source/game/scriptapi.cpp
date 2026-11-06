
// Include
#include "lua.h"
#include "scriptsource.h"

#include "utils/functions.h"

// The game's scripts refer to objects by id and expect the original client's Lua API.
// This file adapts those calls to the functions this server already has. It should only
// translate: real behaviour belongs in C++.
namespace {
	constexpr const char* setupApi = R"lua(
		setmetatable(nAbility, { __index = Ability })

		nBit = nBit or {}
		function nBit.Or(...)
			local value = 0
			for i = 1, select("#", ...) do
				value = bit.bor(value, (select(i, ...)))
			end
			return value
		end

		function nBit.And(...)
			local value = -1
			for i = 1, select("#", ...) do
				value = bit.band(value, (select(i, ...)))
			end
			return value
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

	// Each running ability is a coroutine, so its context follows it across waits.
	constexpr const char* castApi = R"lua(
		local running = coroutine.running
		local casts = setmetatable({}, { __mode = "k" })
		local sequenceSteps = {}
		local nextInstanceId = 0

		local function currentCast()
			return casts[running()]
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
			return target:TakeDamage(nil, snapshot, damage, damageType, damageSource, coefficient or 0, descriptors, multiplier, direction)
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
}

// Game
namespace Game {
	void Lua::RegisterGameScriptApi() {
		sol::table gameAbility = mState["nAbility"];

		gameAbility["RegisterAbility"] = [this](sol::this_state L, const std::string& name, sol::table ability) {
			if (auto [it, inserted] = mAbilities.try_emplace(utils::hash_id(name)); inserted) {
				ability["id"] = it->first;
				it->second = std::make_shared<Ability>(*this, std::move(ability), name, it->first);
			}
		};

		const auto preload = [](const std::string& name, sol::variadic_args) -> uint32_t {
			return utils::hash_id(name);
		};
		gameAbility["PreloadAnimation"] = preload;
		gameAbility["PreloadAsset"] = preload;
		gameAbility["PreloadModifier"] = preload;

		mState["ReadGameScript"] = [](const std::string& path) {
			return ScriptSource::Read(path);
		};

		LoadBuffer(setupApi);
		LoadBuffer(castApi);
		LoadBuffer(objectApi);
	}
}
