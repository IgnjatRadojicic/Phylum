
// Include
#include "game/scriptsource.h"

#include <doctest/doctest.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using Game::ScriptSource;

namespace {
	constexpr uint32_t loadConstantToRegister0 = 1;
	constexpr uint32_t returnRegister0 = 0x0100001E;
	constexpr uint32_t returnNothing = 0x0080001E;

	// Builds a script in the game's format: 4 byte sizes and float numbers.
	class GameScript {
		public:
			GameScript() {
				mData.assign("\x1bLuaQ\x00\x01\x04\x04\x04\x04\x00", 12);
				Add<uint32_t>(0); // source name
				Add<int32_t>(0); // first line
				Add<int32_t>(0); // last line
				mData.append("\x00\x00\x02\x02", 4); // upvalues, parameters, vararg, stack size
				Add<int32_t>(3); // instructions
				Add<uint32_t>(loadConstantToRegister0);
				Add<uint32_t>(returnRegister0);
				Add<uint32_t>(returnNothing);
			}

			std::string WithNumber(float value) {
				Add<int32_t>(1); // constants
				mData.push_back(3);
				Add<float>(value);
				return Finish();
			}

			std::string WithString(const std::string& value) {
				Add<int32_t>(1); // constants
				mData.push_back(4);
				Add<uint32_t>(static_cast<uint32_t>(value.size() + 1));
				mData.append(value.c_str(), value.size() + 1);
				return Finish();
			}

			std::string WithConstantType(char type) {
				Add<int32_t>(1); // constants
				mData.push_back(type);
				return Finish();
			}

		private:
			template<typename T>
			void Add(T value) {
				mData.append(reinterpret_cast<const char*>(&value), sizeof(T));
			}

			std::string Finish() {
				Add<int32_t>(0); // nested functions
				Add<int32_t>(0); // line numbers
				Add<int32_t>(0); // local names
				Add<int32_t>(0); // upvalue names
				return mData;
			}

			std::string mData;
	};

	// Loads bytecode into real Lua 5.1, runs it, and leaves its result on the stack.
	class LuaRun {
		public:
			explicit LuaRun(const std::string& bytecode) : mState(luaL_newstate()) {
				mStatus = luaL_loadbuffer(mState, bytecode.data(), bytecode.size(), "test");
				if (mStatus == 0) {
					mStatus = lua_pcall(mState, 0, 1, 0);
				}
			}

			~LuaRun() {
				lua_close(mState);
			}

			int Status() const { return mStatus; }
			double Number() const { return lua_tonumber(mState, -1); }
			std::string Text() const { return lua_tostring(mState, -1); }

		private:
			lua_State* mState;
			int mStatus = 0;
	};
}

TEST_CASE("recognises the game's bytecode") {
	CHECK(ScriptSource::IsGameBytecode(GameScript().WithNumber(1.f)));
	CHECK_FALSE(ScriptSource::IsGameBytecode("print('hello')"));
	CHECK_FALSE(ScriptSource::IsGameBytecode(""));
}

TEST_CASE("converted bytecode has this build's sizes") {
	const auto converted = ScriptSource::ConvertBytecode(GameScript().WithNumber(1.f));
	CHECK(converted[8] == sizeof(size_t));
	CHECK(converted[10] == sizeof(double));
	CHECK_FALSE(ScriptSource::IsGameBytecode(converted));
}

TEST_CASE("converted number constants run in Lua") {
	LuaRun run(ScriptSource::ConvertBytecode(GameScript().WithNumber(0.5f)));
	REQUIRE(run.Status() == 0);
	CHECK(run.Number() == doctest::Approx(0.5));
}

TEST_CASE("float constants keep the value the game had") {
	LuaRun run(ScriptSource::ConvertBytecode(GameScript().WithNumber(0.4f)));
	REQUIRE(run.Status() == 0);
	CHECK(run.Number() == static_cast<double>(0.4f));
}

TEST_CASE("converted string constants run in Lua") {
	LuaRun run(ScriptSource::ConvertBytecode(GameScript().WithString("SplinteringCleave")));
	REQUIRE(run.Status() == 0);
	CHECK(run.Text() == "SplinteringCleave");
}

TEST_CASE("bad bytecode is rejected") {
	const auto script = GameScript().WithNumber(1.f);

	SUBCASE("cut short") {
		CHECK_THROWS_AS(ScriptSource::ConvertBytecode(script.substr(0, script.size() - 6)), std::runtime_error);
	}

	SUBCASE("extra data at the end") {
		CHECK_THROWS_AS(ScriptSource::ConvertBytecode(script + "x"), std::runtime_error);
	}

	SUBCASE("unknown constant type") {
		CHECK_THROWS_AS(ScriptSource::ConvertBytecode(GameScript().WithConstantType(9)), std::runtime_error);
	}
}

TEST_CASE("reading a script converts bytecode and leaves text alone") {
	const auto path = std::filesystem::temp_directory_path() / "recap_scriptsource_test.lua";

	SUBCASE("bytecode") {
		std::ofstream(path, std::ios::binary) << GameScript().WithNumber(2.f);
		LuaRun run(ScriptSource::Read(path));
		REQUIRE(run.Status() == 0);
		CHECK(run.Number() == doctest::Approx(2.0));
	}

	SUBCASE("text") {
		std::ofstream(path, std::ios::binary) << "return 1";
		CHECK(ScriptSource::Read(path) == "return 1");
	}

	std::filesystem::remove(path);
}
