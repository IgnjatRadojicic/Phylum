
// Include
#include "lua.h"
#include "conditionregistry.h"

#include "instance.h"
#include "scriptsource.h"
#include "objectmanager.h"
#include "serverevent.h"

#include "utils/functions.h"
#include "utils/log.h"

#include <glm/gtx/norm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef _MSC_VER
#include <stdexcept>
#endif

/*
	Darkspore "docs"
		attribute snapshot (
			attribute value = 0x1C + (value * 4)
		)

		ability layout (
			0x000: char namespace[0x50];
			0x05A: bool requiresAgent;
			0x05C: int allowsMovement;
			0x130: int maxStackCount[8];
			0x150: bool noGlobalCooldown;
			0x151: bool shouldPursue;
			0x153: bool alwaysUseCursorPos;
			0x154: bool channelled;
			0x155: bool showChannelBar;
			0x158: uint32_t minimumChannelTimeMs;
			0x15C: bool faceTargetOnCreate;
			0x152: bool finishOnDeath;
			0x160: uint32_t descriptors;
			0x164: uint32_t debuffDescriptors;
			0x168: uint32_t damageType;
			0x16C: uint32_t damageSource;
			0x170: float damageCoefficient;
			0x174: float healingCoefficient;
			0x178: int modifierPriority;
			0x17C: bool deactivateOnInterrupt;
			0x180: guid statusAnimation;
			0x184: asset statusHeadEffect;
			0x188: asset statusBodyEffect;
			0x18C: asset statusFootEffect;
			0x190: int activationType;
			0x194: int deactivationType;
			0x198: int interfaceType;
			0x19C: int cooldownType;
			0x1A0: int targetType;
			0x074: guid reticleEffect;
			0x078: guid icon;
			0x07C: guid localizedGroup;
			0x080: guid localizedName;
			0x0AC: guid lootGroup;
			0x0B0: guid lootDescription[8];
			0x084: guid localizedDescription[8];
			0x0A8: guid localizedOverdriveDescription;
			0x0D0: float cooldown[8];
			0x0F0: float cooldownVariance{8];
			0x110: float range[8];
			0x1A4: int handledEvents;
			0x1A8: tAssetPropertyVector properties;
			0x1B8: int scalingAttribute;
			0x1BC: int primaryAttributeStat;
			0x1C0: guid primaryAttributeStatDelegate;
			0x1C4: float manaCoefficient;
			0x1E4: bool saveOnDehydrate;
			0x1E5: bool rejectable;
			0x0A4: guid localizedShortDescription;

		)
*/


// Game
namespace Game {
	// LuaThread
	LuaThread::LuaThread(Lua& lua) : mLua(lua) {
		mThread = sol::thread::create(mLua.GetState());
	}

	sol::object LuaThread::get_value(const std::string& key) const {
		if (auto it = mValues.find(key); it != mValues.end()) {
			return it->second;
		}
		return sol::make_object(lua_state(), sol::lua_nil);
	}

	void LuaThread::set_value(const std::string& key, sol::object&& value) {
		mValues[key] = value;
	}

	bool LuaThread::resume() {
		if (!mCoroutine.valid()) {
			return true;
		}

		if (mCoroutine.error()) {
			return true;
		}

		sol::variadic_results results;
		if (mResumeCondition && !mResumeCondition(lua_state(), results)) {
			return false;
		}

		mResumeCondition = nullptr;
		const auto onFailure = std::move(mOnNextResumeFailure);
		mOnNextResumeFailure = nullptr;

		sol::protected_function_result result = mCoroutine(sol::as_args(results));
		const bool failed = !result.valid();
		if (failed) {
			sol::error err = result;
			std::cout << "LuaThread::resume()" << std::endl;
			std::cout << err.what() << std::endl;
			report("resume", err.what());
			std::cout << std::to_underlying(mCoroutine.status()) << std::endl;
		}

		const bool done = !is_waiting();
		if (failed && onFailure) {
			onFailure();
		}
		return done;
	}

	void LuaThread::report(const char* where, const std::string& error) {
		mLua.Trace(std::string("ERROR ") + where + " " + error);
	}

	void LuaThread::stop() {
		// Stop coroutine and recreate thread
		mLua.RenewThread(this);
	}

	void LuaThread::set_resume_condition(const ResumeCondition& condition) { mResumeCondition = condition; }
	void LuaThread::set_resume_condition(ResumeCondition&& condition) { mResumeCondition = std::move(condition); }
	void LuaThread::on_next_resume_failure(std::function<void()> handler) { mOnNextResumeFailure = std::move(handler); }

	void LuaThread::create(const sol::function& func) {
		// Reset coroutine
		mCoroutine = sol::nil;
		mCoroutine = sol::coroutine(mThread.thread_state(), sol::ref_index(func.registry_index()));
	}

	void LuaThread::renew() {
		mCoroutine = sol::nil;
		mResumeCondition = nullptr;
		mOnNextResumeFailure = nullptr;
		mHasEnvironment = false;
		mThread = sol::thread::create(mLua.GetState());
	}

	void LuaThread::post_call() {
		if (!is_waiting()) {
			mOnNextResumeFailure = nullptr;
			mLua.ReturnThreadToPool(this);
		} else {
			// A script's own coroutine.yield means "go on next frame" (the time bubble's loop). The
			// wait functions queue their thread themselves; queueing it again changes nothing.
			mLua.YieldThread(this);
		}
	}

	// LuaBase
	LuaBase::~LuaBase() {
		CollectGarbage();
	}

	void LuaBase::Initialize() {
		mState.open_libraries();
		sol::set_default_state(mState);

		// set package path
		LoadBuffer(R"(package.path = package.path .. ";./data/serverdata/lua/?.lua")");

		// Get default packages
		sol::table packagesLoaded = mState["package"]["loaded"];
		for (const auto& [name, _] : packagesLoaded) {
			mDefaultPackages.push_back(name.as<std::string>());
		}

		LoadAssetsCatalog();

		// register base functions
		RegisterFunctions();
	}

	void LuaBase::Reload() {
		auto end = mDefaultPackages.end();

		sol::table packagesLoaded = mState["package"]["loaded"];
		for (const auto& [k, v] : packagesLoaded) {
			const auto& name = k.as<std::string>();
			if (auto it = std::find(mDefaultPackages.begin(), end, name); it != end) {
				continue;
			}
			packagesLoaded[k] = sol::nil;
		}

		CollectGarbage();
	}

	sol::protected_function_result LuaBase::ReportError(lua_State* L, sol::protected_function_result pfr) {
		sol::error err = pfr;

		/*
		if (function) {
			printf("%s().\n", function);
		}
		*/

		bool stackTrace = true;
		if (stackTrace) {
			auto trace = GetStackTrace(L, err.what());
			std::cout << trace << std::endl;
		} else {
			// std::string file = getFileById(runningEvent);
			// printf("%s\n%s\n", file.c_str(), err.what());
		}

		return pfr;
	}

	const char* LuaBase::GetStackTrace(lua_State* L, const char* error) {
		lua_getfield(L, LUA_GLOBALSINDEX, "debug");
		if (lua_type(L, -1) != LUA_TTABLE) {
			lua_pop(L, 1);
			return error;
		}

		lua_getfield(L, -1, "traceback");
		if (lua_type(L, -1) != LUA_TFUNCTION) {
			lua_pop(L, 1);
			return error;
		}

		lua_pushstring(L, error);
		lua_call(L, 1, 1);

		const char* result = lua_tostring(L, -1);
		lua_pop(L, 1);

		return result;
	}

	sol::protected_function_result LuaBase::LoadFile(const std::string& path) {
		return mState.safe_script(ScriptSource::Read(path), &LuaBase::ReportError, "@" + path);
	}

	sol::protected_function_result LuaBase::LoadBuffer(const std::string& buffer) {
		return mState.safe_script(buffer, &LuaBase::ReportError);
	}

	lua_State* LuaBase::GetState() const {
		return mState;
	}

	void LuaBase::CollectGarbage() {
		mState.collect_garbage();
		mState.collect_garbage();
	}

	void LuaBase::LogError(lua_State* L, std::string_view function, const std::string& message) const {
		std::cout
			<< "[" << utils::timestamp << "]"
			<< "[Lua error]: "
			<< utils::yellow << function << utils::normal << ": "
			<< message << "."
			<< std::endl;
	}

	// GlobalLua
	decltype(GlobalLua::sInstance) GlobalLua::sInstance;

	GlobalLua::~GlobalLua() {
		//
	}

	GlobalLua& GlobalLua::Instance() {
		return sInstance;
	}

	void GlobalLua::Initialize() {
		LuaBase::Initialize();

		// Load defaults & abilities
		LoadFile("data/serverdata/lua/global.lua");
		LoadAbilities();
	}

	void GlobalLua::Reload() {
		// temporary
		LuaBase::Reload();
		mLoadedAbilities.clear();
		LoadAbilities();
	}

	const sol::bytecode& GlobalLua::GetAbility(const std::string& abilityName) const {
		return GetAbility(utils::hash_id(abilityName));
	}

	const sol::bytecode& GlobalLua::GetAbility(uint32_t abilityId) const {
		static thread_local sol::bytecode emptyBytecode {};
		auto it = mLoadedAbilities.find(abilityId);
		return it != mLoadedAbilities.end() ? it->second : emptyBytecode;
	}

	const std::unordered_map<uint32_t, sol::bytecode>& GlobalLua::GetLoadedAbilities() const {
		return mLoadedAbilities;
	}

	void GlobalLua::LoadAbilities() {
		const auto& dataPath = "data/serverdata/Abilities/";
		if (!std::filesystem::exists(dataPath)) {
			std::cout << "Cannot load scripts." << std::endl;
			return;
		}

		std::cout << "Loading ability scripts...";

		size_t failed = 0;
		for (const auto& entry : std::filesystem::directory_iterator(dataPath)) {
			const auto& path = entry.path();
			if (entry.is_regular_file() && path.extension() == ".lua") {
				auto result = mState.load(ScriptSource::Read(path), "@" + path.string());
				if (!result.valid()) {
					if (failed++ == 0) {
						std::cout << std::endl;
					}

					sol::error err = result;
					std::string what = err.what();

					std::cout << "GlobalLua: Could not load " << path << "." << std::endl;
					std::cout << err.what() << std::endl << std::endl;
					continue;
				}
				
				const auto& [it, inserted] = mLoadedAbilities.try_emplace(utils::hash_id(path.stem().string()));
				if (inserted) {
					sol::protected_function target = result.get<sol::protected_function>();
					it->second = target.dump();
				}
			}
		}

		if (mLoadedAbilities.empty() && failed > 0) {
			std::cout << " failed." << std::endl;
		} else {
			std::cout << " done. (loaded: " << mLoadedAbilities.size() << ", failed: " << failed << ")" << std::endl;
		}
	}

	// Lua
	Lua::Lua(Instance& game) : LuaBase(), mGame(game),
		mConditions(std::make_unique<ConditionRegistry>([this](const std::string& line) { Trace(line); })) {}
	Lua::~Lua() {
		while (!mThreads.empty()) {
			auto it = mThreads.begin();
			delete it->second;
			mThreads.erase(it);
		}
	}

	void Lua::Initialize() {
		LuaBase::Initialize();

		RegisterEnums();
		RegisterFunctions();
		RegisterGameScriptApi();

		LoadFile("data/serverdata/lua/global.lua");
	}

	void Lua::Reload() {
		LuaBase::Reload();
		GlobalLua::Instance().Reload();
		mAbilities.clear();
	}

	void Lua::Update() {
		// A resumed script can start threads of its own, so the set may change along the way.
		const std::vector<LuaThread*> threads(mYieldedThreads.begin(), mYieldedThreads.end());
		for (auto thread : threads) {
			if (mYieldedThreads.contains(thread) && thread->resume()) {
				mYieldedThreads.erase(thread);
				ReturnThreadToPool(thread);
			}
		}
	}

	void Lua::PreloadLibraries() {
		// These have hashed names, so no script can require them, but scripts call what they define.
		const std::filesystem::path folder = "data/serverdata/lua";
		if (!std::filesystem::exists(folder)) {
			return;
		}

		sol::protected_function require = mState["require"];
		for (const auto& entry : std::filesystem::directory_iterator(folder)) {
			const auto name = entry.path().filename().string();
			if (name.starts_with("0x") && entry.path().extension() == ".lua") {
				sol::protected_function_result result = require("Lua!" + name);
				if (!result.valid()) {
					sol::error err = result;
					std::cout << err.what() << std::endl;
				}
			}
		}
	}

	void Lua::PreloadConditions() {
		// The AI's conditions, one per file (Condition_Closest.lua); a few libraries add more.
		const std::filesystem::path folder = "data/serverdata/lua";
		if (!std::filesystem::exists(folder)) {
			return;
		}

		sol::protected_function require = mState["require"];

		size_t failed = 0;
		for (const auto& entry : std::filesystem::directory_iterator(folder)) {
			const auto name = entry.path().filename().string();
			if (entry.path().extension() != ".lua" || !utils::string_iequals(std::string_view(name).substr(0, 10), "condition_")) {
				continue;
			}

			sol::protected_function_result result = require("Lua!" + name);
			if (!result.valid()) {
				sol::error err = result;
				std::cout << err.what() << std::endl;
				++failed;
			}
		}
		std::cout << "Game condition scripts: " << mConditions->Size() << " registered, " << failed << " failed." << std::endl;
	}

	ConditionRegistry& Lua::GetConditions() {
		return *mConditions;
	}

	void Lua::PreloadBehaviors() {
		// The AI's behaviors (nBehavior_FollowOwner), one global table per file (BehaviorRunner).
		const std::filesystem::path folder = "data/serverdata/behaviors";
		if (!std::filesystem::exists(folder)) {
			return;
		}

		sol::protected_function require = mState["require"];

		size_t loaded = 0, failed = 0;
		for (const auto& entry : std::filesystem::directory_iterator(folder)) {
			if (entry.path().extension() != ".lua") {
				continue;
			}

			sol::protected_function_result result = require("behaviors!" + entry.path().filename().string());
			if (result.valid()) {
				++loaded;
			} else {
				sol::error err = result;
				std::cout << err.what() << std::endl;
				++failed;
			}
		}
		std::cout << "Game behavior scripts: " << loaded << " loaded, " << failed << " failed." << std::endl;
	}

	void Lua::PreloadAbilities() {
		PreloadLibraries();
		PreloadConditions();
		PreloadBehaviors();

		// Each script registers itself under its own name when it runs.
		const auto run = [this](const sol::bytecode& code, bool report) {
			sol::protected_function_result result = mState.safe_script(code.as_string_view(), &sol::script_pass_on_error);
			if (!result.valid() && report) {
				sol::error err = result;
				std::cout << err.what() << std::endl;
			}
			return result.valid();
		};

		std::vector<const sol::bytecode*> retry;
		for (const auto& [_, code] : GlobalLua::Instance().GetLoadedAbilities()) {
			if (!run(code, false)) {
				retry.push_back(&code);
			}
		}

		PreloadModifiers();

		// Some abilities read their modifier's table while they load (Blitz's LightningRogueSupport
		// reads nModifier_LightningRogueSupport), and that only exists once the modifiers ran.
		size_t failed = 0;
		for (const auto* code : retry) {
			failed += !run(*code, true);
		}
		std::cout << "Game ability scripts: " << mAbilities.size() << " registered, " << failed << " failed." << std::endl;

		// These only exist once a script has finished running, not when it registers.
		for (const auto& [_, ability] : mAbilities) {
			ability->FindNumberedTick();
		}
	}

	void Lua::PreloadModifiers() {
		// Abilities only load the modifiers they require, and some belong to nothing else.
		const std::filesystem::path folder = "data/serverdata/Modifiers";
		if (!std::filesystem::exists(folder)) {
			return;
		}

		sol::protected_function require = mState["require"];

		size_t failed = 0;
		for (const auto& entry : std::filesystem::directory_iterator(folder)) {
			if (entry.path().extension() != ".lua") {
				continue;
			}

			sol::protected_function_result result = require("Modifiers!" + entry.path().filename().string());
			if (!result.valid()) {
				sol::error err = result;
				std::cout << err.what() << std::endl;
				++failed;
			}
		}

		// Keyed by name, so size() (array length) would always be 0.
		size_t registered = 0;
		for (const auto& _ : mState["nModifier"]["registered"].get<sol::table>()) {
			++registered;
		}
		std::cout << "Game modifier scripts: " << registered << " registered, " << failed << " failed." << std::endl;
	}

	Instance& Lua::GetGame() {
		return mGame;
	}

	const Instance& Lua::GetGame() const {
		return mGame;
	}

	AbilityPtr Lua::GetAbility(const std::string& abilityName) {
		return GetAbility(utils::hash_id(abilityName));
	}

	AbilityPtr Lua::GetAbility(uint32_t abilityId) {
		const auto it = mAbilities.find(abilityId);
		return it != mAbilities.end() ? it->second : nullptr;
	}

	sol::table Lua::GetPrivateTable(uint32_t id) const {
		if (auto it = mPrivateTables.find(id); it != mPrivateTables.end()) {
			return it->second;
		}
		return sol::nil;
	}

	sol::table Lua::CreatePrivateTable(uint32_t id) {
		auto [it, inserted] = mPrivateTables.try_emplace(id);
		if (inserted) {
			it->second = mState.create_table();
		}
		return it->second;
	}

	void Lua::RemovePrivateTable(uint32_t id) {
		mPrivateTables.erase(id);
	}

	LuaThread* Lua::SpawnThread() {
		LuaThread* thread;
		if (mThreadPool.empty()) {
			thread = new LuaThread(*this);
			mThreads.emplace(thread->lua_state(), thread);
		} else {
			thread = mThreadPool.back();
			mThreadPool.pop_back();
		}
		return thread;
	}

	LuaThread* Lua::GetThread(lua_State* L) const {
		if (auto it = mThreads.find(L); it != mThreads.end()) {
			return it->second;
		}
		return nullptr;
	}

	void Lua::YieldThread(LuaThread* thread) {
		if (thread) {
			mYieldedThreads.insert(thread);
		}
	}

	LuaThread* Lua::SpawnThreadFor(const ObjectPtr& object) {
		auto thread = SpawnThread();
		thread->mOwner = object;
		object->SetLuaThread(thread);
		return thread;
	}

	void Lua::RenewThread(LuaThread* thread) {
		mThreads.erase(thread->lua_state());
		thread->renew();
		mThreads.emplace(thread->lua_state(), thread);
	}

	uint64_t Lua::TrackThread(LuaThread* thread) {
		// One ticket per thread, so a thread tracked twice can still be found by the first.
		if (thread->mTicket != 0) {
			return thread->mTicket;
		}

		const auto ticket = mNextThreadTicket++;
		thread->mTicket = ticket;
		mTrackedThreads.emplace(ticket, thread);
		return ticket;
	}

	void Lua::StopThread(uint64_t ticket) {
		const auto it = mTrackedThreads.find(ticket);
		if (it == mTrackedThreads.end()) {
			return;
		}

		// A thread cannot be stopped from inside itself, and it ends by itself in that case.
		auto thread = it->second;
		if (lua_status(thread->lua_state()) == LUA_YIELD) {
			thread->mTicket = 0;
			mTrackedThreads.erase(it);
			CancelThread(thread);
		}
	}

	bool Lua::IsThreadRunning(uint64_t ticket) const {
		return ticket != 0 && mTrackedThreads.contains(ticket);
	}

	void Lua::CancelThread(LuaThread* thread) {
		if (thread && lua_status(thread->lua_state()) == LUA_YIELD) {
			// Or Update would resume it after it is pooled, or deleted.
			mYieldedThreads.erase(thread);
			ReturnThreadToPool(thread);
		}
	}

	void Lua::ReturnThreadToPool(LuaThread* thread) {
		sol::main_table cast = std::move(thread->mCastContext);
		thread->mCastContext = sol::main_table();

		// Scripts key their cast by thread (see CastContext in scriptapi.cpp), and this one will be reused.
		if (sol::optional<sol::protected_function> forget = mState["CastContext"]["Forget"]) {
			(*forget)(thread->mThread);
		}

		if (thread->mTicket != 0) {
			mTrackedThreads.erase(thread->mTicket);
			thread->mTicket = 0;
		}

		// The thread may be reused or deleted from here on, so no object may keep pointing at it.
		if (auto owner = thread->mOwner.lock(); owner && owner->GetLuaThread() == thread) {
			owner->SetLuaThread(nullptr);
		}
		thread->mOwner.reset();

		// A Lua thread that failed or was cut off while waiting can never run again, and one
		// given its own globals would pass them on to whatever runs on it next. 0 is a finished one.
		if (lua_status(thread->lua_state()) != 0 || thread->mHasEnvironment) {
			RenewThread(thread);
		}

		constexpr size_t MAX_LUA_THREADS = 256;
		if (mThreads.size() > MAX_LUA_THREADS) {
			mThreads.erase(thread->lua_state());
			delete thread;
		} else {
			thread->mValues.clear(); // Remove any stored values.
			mThreadPool.push_back(thread);
		}

		// Last, because this thread is free again and the script can start others.
		if (cast.valid()) {
			sol::function endCast = mState["nAbility"]["EndCast"];
			if (!SpawnThread()->call<void>(endCast, sol::table(cast))) {
				// A Deactivate that failed stopped EndCast before the cast's changes came off.
				sol::function endChanges = mState["nAbility"]["EndCastChanges"];
				SpawnThread()->call<void>(endChanges, sol::table(cast));
			}
		}
		// CollectGarbage();
	}

	// Coroutine
	Coroutine::Coroutine(Lua& lua, sol::table&& self) : mLua(lua), mSelf(std::move(self)) {
		if (mSelf == sol::nil) {
			throw std::runtime_error("Coroutine::Coroutine: self is nil");
		}

		sol::state_view state = mSelf.lua_state();
		mEnvironment = sol::environment(state, sol::create, state.globals());
	}

	void Coroutine::Reload() {
		// TODO:
	}

	// Ability
	Ability::Ability(Lua& lua, sol::table&& self, const std::string& name, uint32_t id) : Coroutine(lua, std::move(self)), mName(name), mId(id) {
		mEnvironment["_ABILITY"] = this;

		if (sol::optional<bool> hasActivate = mSelf["hasActivate"]; hasActivate.value_or(false)) {
			if (sol::object value = mSelf["activate"]; value.is<sol::function>()) {
				mEnvironment.set_on(value);
				mActivateFn = value.as<sol::function>();
			}
		}

		if (sol::optional<bool> hasDeactivate = mSelf["hasDeactivate"]; hasDeactivate.value_or(false)) {
			if (sol::object value = mSelf["deactivate"]; value.is<sol::function>()) {
				mEnvironment.set_on(value);
				mDeactivateFn = value.as<sol::function>();
			}
		}

		if (sol::optional<bool> hasTick = mSelf["hasTick"]; hasTick.value_or(false)) {
			// castTick wraps the script's tick with the cast setup (see scriptapi.cpp).
			if (sol::object value = mSelf["castTick"]; value.is<sol::function>()) {
				mEnvironment.set_on(value);
				mTickFn = value.as<sol::function>();
			}
		}

		// Fetch properties
		mDescriptors = mSelf.get_or("descriptors", Descriptors::IsBasic);
		mScalingAttribute = mSelf.get_or("scalingAttribute", utils::enum_wrapper<AttributeType>().bnot().value());
		mInterface = mSelf.get_or("interfaceType", InterfaceType::Position);
		mRequiresAgent = mSelf.get_or("requiresAgent", false);
		mHasGlobalCooldown = mSelf.get_or("noGlobalCooldown", false) == false;
		mShouldPursue = mSelf.get_or("shouldPursue", false);
	}

	const std::string& Ability::GetName() const {
		return mName;
	}

	Descriptors Ability::GetDescriptors() const {
		return mDescriptors;
	}

	AttributeType Ability::GetScalingAttribute() const {
		return mScalingAttribute;
	}

	InterfaceType Ability::GetInterface() const {
		return mInterface;
	}

	uint32_t Ability::GetId() const {
		return mId;
	}

	bool Ability::RequiresAgent() const {
		return mRequiresAgent;
	}

	bool Ability::HasGlobalCooldown() const {
		return mHasGlobalCooldown;
	}

	bool Ability::ShouldPursue() const {
		return mShouldPursue;
	}

	AbilityCostData Ability::GetCostData(const ObjectPtr& object, int32_t rank) const {
		AbilityCostData data;
		data.basic = utils::enum_wrapper { mDescriptors }.test(Descriptors::IsBasic);

		sol::state_view state(mLua.GetState());
		sol::protected_function read = state["AbilityApi"]["CostData"];
		sol::protected_function_result result = read(mSelf, object ? object->GetId() : 0, std::max(1, rank));
		if (!result.valid()) {
			sol::error error = result;
			mLua.Trace("ERROR cost ability=" + std::to_string(mId) + " " + error.what());
			return data;
		}

		data.manaCost = result.get<float>(0);
		data.manaCoefficient = result.get<float>(1);
		data.cooldownSeconds = result.get<float>(2);
		data.manual = result.get<bool>(3);
		data.cooldownVariance = result.get<float>(4);
		return data;
	}

	float Ability::GetRange(int32_t rank) const {
		sol::state_view state(mLua.GetState());
		sol::protected_function read = state["AbilityApi"]["Range"];
		sol::protected_function_result result = read(mSelf, std::max(1, rank));
		if (!result.valid()) {
			sol::error error = result;
			mLua.Trace("ERROR range ability=" + std::to_string(mId) + " " + error.what());
			return 0.f;
		}
		return result.get<float>(0);
	}

	bool Ability::IsInRange(const ObjectPtr& object, const ObjectPtr& target, const glm::vec3& targetPosition, int32_t rank) const {
		if (!object) {
			return true;
		}

		const auto descriptors = utils::enum_wrapper { mDescriptors };

		float range = 0.f;
		if (range <= 0.f && descriptors.test(Descriptors::IsMelee)) {
			range = 2.f; // This is the defined range in darkspore
		}

		if (const auto& attributes = object->GetAttributeData()) {
			if (descriptors.test(Descriptors::IsMelee, Descriptors::IsBasic)) {
				range *= 1 + attributes->GetValue(AttributeType::RangeIncrease);
			}
			
			if (descriptors.test(Descriptors::IsAoE)) {
				range *= 1 + attributes->GetValue(AttributeType::AoeRadius);
			}
		}

		if (range <= 0.f) {
			return true;
		}

		if (object->IsPlayerControlled()) {
			//
		}

		if (target) {
			// unused currently
			const auto modifiedSpeed = target->GetModifiedMovementSpeed();

			//
			auto distance = glm::distance(object->GetPosition(), target->GetPosition());
			distance -= object->GetFootprintRadius() + target->GetFootprintRadius();

			if (range < distance) {
				return true;
			}
		}

		switch (mInterface) {
			case InterfaceType::Position:
			case InterfaceType::TerrainPoint:
				return false;
		}

		auto distance = glm::distance(object->GetPosition(), targetPosition);
		distance -= object->GetFootprintRadius();

		return range < distance;
	}

	bool Ability::IsAbleToHit(const ObjectPtr& object, const ObjectPtr& target, const glm::vec3& targetPosition, int32_t rank) const {
		sol::protected_function method = mSelf["IsAbleToHit"];
		if (method != sol::nil) {
			return method.call<bool>(mSelf, object, target, targetPosition, rank);
		}
		return mLua.GetGame().GetObjectManager().IsInLineOfSight(object, target, targetPosition);
	}

	// The numbered tick, when a script sets one, is the ability's real entry point and
	// wins over the named one. Most just call self:tick(), but some do more first
	// (FearNova removes a modifier, SoulRavagerBasic passes its soul count) and some
	// replace it entirely (LightningTempest_Active's volley never calls self:tick()).
	void Ability::FindNumberedTick() {
		sol::state_view state(mLua.GetState());
		sol::protected_function find = state["nAbility"]["NumberedTick"];
		sol::protected_function_result result = find(mSelf);
		if (!result.valid()) {
			return;
		}

		if (sol::object value = result; value.is<sol::function>()) {
			mEnvironment.set_on(value);
			mTickFn = value.as<sol::function>();
		}
	}

	bool Ability::Tick(ObjectPtr object, ObjectPtr target, glm::vec3 cursorPosition, int32_t rank) const {
		if (!mTickFn) {
			return false;
		}

		// A script can run long after its hero is free, while a shot flies, so it never blocks a new cast.
		LuaThread* thread = object ? mLua.SpawnThreadFor(object) : mLua.SpawnThread();

		thread->call<void>(mEnvironment, mTickFn, mSelf, object, target, cursorPosition, rank);
		return true;
	}

	// Objective
	Objective::Objective(Lua& lua, sol::table&& self) : Coroutine(lua, std::move(self)) {
		mEnvironment["_OBJECTIVE"] = this;

		if (sol::object value = mSelf["init"]; value.is<sol::function>()) {
			mEnvironment.set_on(value);
			mInitFn = value.as<sol::function>();
		}

		if (sol::object value = mSelf["handle_event"]; value.is<sol::function>()) {
			mEnvironment.set_on(value);
			mHandleEventFn = value.as<sol::function>();
		}

		if (sol::object value = mSelf["status"]; value.is<sol::function>()) {
			mEnvironment.set_on(value);
			mStatusFn = value.as<sol::function>();
		}
	}
}
