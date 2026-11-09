
#ifndef _GAME_LUA_HEADER
#define _GAME_LUA_HEADER

// Include
#include "abilitycost.h"
#include "attributes.h"
#include "runningcasts.h"

#include <fstream>
#include <glm/glm.hpp>

#define SOL_SAFE_FUNCTION_OBJECTS 1
#define SOL_PRINT_ERRORS 1
#include <sol/sol.hpp>

#include <unordered_map>
#include <unordered_set>
#include <functional>

// Game
namespace Game {
	// Predefined
	class Instance;
	class LuaBase;
	class Lua;
	class Ability;
	class ConditionRegistry;

	using AbilityPtr = std::shared_ptr<Ability>;

	// LuaThread
	class LuaThread {
		public:
			using ResumeCondition = std::function<bool(lua_State*, sol::variadic_results&)>;

		public:
			LuaThread(Lua& lua);

			decltype(auto) lua_state() const { return mThread.thread_state(); }
			decltype(auto) status() const { return mThread.status(); }

			// A finished or failed thread is free. Only one waiting to resume is still in use.
			bool is_waiting() const { return mCoroutine.valid() && mCoroutine.status() == sol::call_status::yielded; }

			// When the thread was given its function. WaitUntilTime counts from it (client 0xA077E0
			// compares the time since thread+0xE8), while WaitForXSeconds counts from the call.
			uint64_t started_ms() const { return mStartMs; }

			sol::object get_value(const std::string& key) const;
			void set_value(const std::string& key, sol::object&& value);

			bool resume();
			void stop();

			void set_resume_condition(const ResumeCondition& condition);
			void set_resume_condition(ResumeCondition&& condition);

			// Runs once if the next resume fails, then is dropped (also when the thread goes back to
			// the pool). A modifier's Tick starts on that resume (ModifierApi.Start waits one update).
			void on_next_resume_failure(std::function<void()> handler);

			// call<void> answers whether the script ran without an error. Use that answer, not
			// this thread: a thread that did not wait is back in the pool, or deleted, on return.
			template<typename Result, typename... Args>
			decltype(auto) call(const sol::function& func, Args&&... args) {
				create(func);

				sol::protected_function_result result = mCoroutine(std::forward<Args>(args)...);
				const bool succeeded = result.valid();
				if (!succeeded) {
					sol::error err = result;
					std::cout << "LuaThread::call()" << std::endl;
					std::cout << err.what() << std::endl;
					report("call", err.what());
					std::cout << std::to_underlying(mCoroutine.status()) << std::endl;
				}
				post_call();

				if constexpr (!std::is_void_v<Result>) {
					return static_cast<Result>(result);
				} else {
					return succeeded;
				}
			}

			template<typename Result, typename... Args>
			auto call(const sol::environment& env, const sol::function& func, Args&&... args) {
				env.set_on(mThread);
				mHasEnvironment = true;
				return call<Result>(func, std::forward<Args>(args)...);
			}

		private:
			void create(const sol::function& func);
			void renew();
			void post_call();
			void report(const char* where, const std::string& error);
		
		private:
			Lua& mLua;
			
			std::unordered_map<std::string, sol::object> mValues;

			// The object casting on this thread, whose pointer is cleared when the thread is freed.
			std::weak_ptr<Object> mOwner;
			uint64_t mTicket = 0;

			// Set when a call gave the thread its own globals, which the next user must not inherit.
			bool mHasEnvironment = false;

			// A cast running on this thread, whose deactivate runs when the thread is freed.
			sol::main_table mCastContext;

			// Asked to end while it was running (Lua::EndCast), so it ends when it next waits.
			bool mEndRequested = false;

			ResumeCondition mResumeCondition;
			uint64_t mStartMs = 0;
			std::function<void()> mOnNextResumeFailure;

			sol::thread mThread;
			sol::coroutine mCoroutine;
			sol::environment mEnvironment;

			friend class Lua;
	};

	// LuaBase
	class LuaBase {
		public:
			LuaBase() = default;
			virtual ~LuaBase();

			virtual void Initialize();
			virtual void Reload();

			static sol::protected_function_result ReportError(lua_State* L, sol::protected_function_result pfr);
			static const char* GetStackTrace(lua_State* L, const char* error);

			sol::protected_function_result LoadFile(const std::string& path);
			sol::protected_function_result LoadBuffer(const std::string& buffer);

			lua_State* GetState() const;

			void CollectGarbage();

		protected:
			void LogError(lua_State* L, std::string_view function, const std::string& message) const;

		private:
			// luafunctions.cpp
			void RegisterFunctions();

			void RegisterMath();
			void RegisterUtil();
			void RegisterClient();
			void RegisterAbility();
			void RegisterPhysics();
			void RegisterGameObject();
			void RegisterAbilityAnimationSelection();
			void RegisterDebug();
			void RegisterNPCType();
			void RegisterTuning();
			void RegisterPlayer();
			void RegisterLocomotion();

			void LoadAssetsCatalog();

		protected:
			sol::state mState;

		private:
			std::unordered_map<std::string, std::string> mAssets;

			std::vector<std::string> mDefaultPackages;
	};

	// GlobalLua
	class GlobalLua : public LuaBase {
		public:
			~GlobalLua();

			static GlobalLua& Instance();

			void Initialize() override;
			void Reload() override;

			const sol::bytecode& GetAbility(const std::string& abilityName) const;
			const sol::bytecode& GetAbility(uint32_t abilityId) const;

			const std::unordered_map<uint32_t, sol::bytecode>& GetLoadedAbilities() const;

		private:
			void LoadAbilities();

		private:
			static GlobalLua sInstance;

			std::unordered_map<uint32_t, sol::bytecode> mLoadedAbilities;
	};

	// Lua
	class Lua : public LuaBase {
		public:
			Lua(Instance& game);
			~Lua();

			void Initialize() override;
			void Reload() override;

			void Update();
			void PreloadAbilities();

			Instance& GetGame();
			const Instance& GetGame() const;

			AbilityPtr GetAbility(const std::string& abilityName);
			AbilityPtr GetAbility(uint32_t abilityId);

			// The AI's conditions, which scripts register through nCondition.
			ConditionRegistry& GetConditions();

			void RegisterGameScriptApi();
			void SetGapLogPath(const std::string& path);
			void SetTracePath(const std::string& path);

			// One line of what scripts did, for reading after a session. Script errors only reach the
			// console otherwise, and each line is flushed so a crash keeps it.
			void Trace(const std::string& line);

			// Also hands each trace line to a check (the hero ability sweep). Empty to stop.
			void SetTraceObserver(std::function<void(const std::string&)> observer);

			sol::table GetPrivateTable(uint32_t id) const;
			sol::table CreatePrivateTable(uint32_t id);
			void RemovePrivateTable(uint32_t id);

			LuaThread* SpawnThread();
			LuaThread* SpawnThreadFor(const ObjectPtr& object);
			void RenewThread(LuaThread* thread);
			LuaThread* GetThread(lua_State* L) const;
			void YieldThread(LuaThread* thread);

			template<typename Result, typename... Args>
			auto CallCoroutine(std::nullptr_t, const sol::function& func, Args&&... args) {
				return SpawnThread()->call<Result>(func, std::forward<Args>(args)...);
			}

			template<typename Result, typename... Args>
			auto CallCoroutine(const sol::environment& env, const sol::function& func, Args&&... args) {
				return SpawnThread()->call<Result>(env, func, std::forward<Args>(args)...);
			}

			void ReturnThreadToPool(LuaThread* thread);
			// Ends a waiting thread at once, as if its script had finished: its cast ends, which runs
			// the ability's Deactivate step where scripts undo their stat changes, and the thread goes
			// back to the pool. A running thread cannot be ended from inside itself.
			void CancelThread(LuaThread* thread);

			// Ends a cast as CancelThread does. A cast that is running now, because it is the one
			// asking, ends when it next waits, as the client defers an instance it is inside (0x9de9c0).
			void EndCast(LuaThread* thread);

			// The casts each agent has running, for the events that reach them.
			const RunningCasts& GetRunningCasts() const;
			sol::table GetCastContext(LuaThread* thread) const;

			// A pooled thread can be reused or deleted, so anything holding on to one keeps a ticket.
			uint64_t TrackThread(LuaThread* thread);
			void StopThread(uint64_t ticket);
			// Whether the tracked thread still waits to go on; a finished one is back in the pool.
			bool IsThreadRunning(uint64_t ticket) const;
			// Whether the tracked thread is running now, not waiting: it is on the call stack.
			bool IsThreadExecuting(uint64_t ticket) const;

		private:
			// luafunctions.cpp
			void PreloadLibraries();
			void PreloadConditions();
			void PreloadBehaviors();
			void PreloadModifiers();

			void RegisterEnums();
			void RegisterFunctions();

			void RegisterThread();
			void RegisterGame();
			void RegisterObjectManager();
			void RegisterLocomotion();
			void RegisterPlayer();
			void RegisterAttributes();
			void RegisterObject();
			void RegisterAbility();
			void RegisterObjective();
			void RegisterTriggerVolume();

		private:
			Instance& mGame;

			std::string mGapLogPath = "script-missing.txt";
			std::string mTracePath = "script-live.txt";
			std::ofstream mTrace;
			std::function<void(const std::string&)> mTraceObserver;

			std::unordered_map<lua_State*, LuaThread*> mThreads;
			std::unordered_map<uint32_t, AbilityPtr> mAbilities;
			std::unordered_map<uint32_t, sol::table> mPrivateTables;
			std::unique_ptr<ConditionRegistry> mConditions;

			std::unordered_set<LuaThread*> mYieldedThreads;
			std::unordered_map<uint64_t, LuaThread*> mTrackedThreads;
			RunningCasts mRunningCasts;
			uint64_t mNextThreadTicket = 1;

			std::vector<LuaThread*> mThreadPool;
	};

	// Coroutine
	class Coroutine {
		public:
			Coroutine(Lua& lua, sol::table&& self);
			virtual ~Coroutine() = default;

			void Reload();

			// The globals its script functions run with (an ability's has _ABILITY).
			const sol::environment& GetEnvironment() const { return mEnvironment; }

		protected:
			template<typename Result, typename... Args>
			auto Call(sol::function fn, Args&&... args) const {
				if (fn) {
					return mLua.CallCoroutine<Result>(mEnvironment, fn, mSelf, std::forward<Args>(args)...);
				}

				// A void call answers whether it ran (see LuaThread::call), so it needs an answer here too.
				if constexpr (std::is_void_v<Result>) {
					return false;
				} else {
					return Result {};
				}
			}

		protected:
			Lua& mLua;

			sol::table mSelf;
			sol::environment mEnvironment;
	};

	// Ability
	class Ability : public Coroutine {
		public:
			Ability(Lua& lua, sol::table&& self, const std::string& name, uint32_t id);

			void FindNumberedTick();

			// properties
			const std::string& GetName() const;

			Descriptors GetDescriptors() const;
			AttributeType GetScalingAttribute() const;
			InterfaceType GetInterface() const;

			uint32_t GetId() const;

			bool RequiresAgent() const;
			bool HasGlobalCooldown() const;
			bool ShouldPursue() const;

			// methods
			// What the script says a cast at this rank costs, before the caster's attributes.
			AbilityCostData GetCostData(const ObjectPtr& object, int32_t rank) const;
			// The script's range at this rank, 0 when it has none.
			float GetRange(int32_t rank) const;

			bool IsInRange(const ObjectPtr& object, const ObjectPtr& target, const glm::vec3& targetPosition, int32_t rank) const;
			bool IsAbleToHit(const ObjectPtr& object, const ObjectPtr& target, const glm::vec3& targetPosition, int32_t rank) const;

			// events
			template<typename... Args>
			auto Activate(Args&&... args) const {
				return Call<void>(mActivateFn, std::forward<Args>(args)...);
			}

			template<typename... Args>
			auto Deactivate(Args&&... args) const {
				return Call<void>(mDeactivateFn, std::forward<Args>(args)...);
			}

			bool Tick(ObjectPtr object, ObjectPtr target, glm::vec3 cursorPosition, int32_t rank) const;

		private:
			std::string mName {};

			Descriptors mDescriptors;
			AttributeType mScalingAttribute;
			InterfaceType mInterface;

			uint32_t mId { 0 };

			bool mRequiresAgent { false };
			bool mHasGlobalCooldown { false };
			bool mShouldPursue { false };

			sol::function mActivateFn;
			sol::function mDeactivateFn;
			sol::function mTickFn;
	};

	// Objective
	class Objective : public Coroutine {
		public:
			Objective(Lua& lua, sol::table&& self);

			template<typename... Args>
			auto Init(Args&&... args) const {
				return Call<void>(mInitFn, std::forward<Args>(args)...);
			}

			template<typename... Args>
			auto HandleEvent(Args&&... args) const {
				return Call<void>(mHandleEventFn, std::forward<Args>(args)...);
			}

			template<typename... Args>
			auto Status(Args&&... args) const {
				return Call<uint32_t>(mStatusFn, std::forward<Args>(args)...);
			}

		private:
			sol::function mInitFn;
			sol::function mHandleEventFn;
			sol::function mStatusFn;
	};
}

#endif
