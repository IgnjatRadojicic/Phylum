
// Include
#include "scriptsource.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

// Game
namespace Game {
	namespace {
		// Lua 5.1 header: signature, version, format, little endian, then the sizes of
		// int, size_t, instruction and number, then whether numbers are integers.
		constexpr char gameHeader[] = "\x1bLuaQ\x00\x01\x04\x04\x04\x04\x00";
		constexpr size_t headerSize = 12;

		enum class ConstantType : uint8_t {
			Nil = 0,
			Boolean = 1,
			Number = 3,
			String = 4
		};

		class BytecodeConverter {
			public:
				explicit BytecodeConverter(const std::string& data) : mData(data) {}

				std::string Convert() {
					mPosition = headerSize;

					mOutput.assign(gameHeader, headerSize);
					mOutput[8] = static_cast<char>(sizeof(size_t));
					mOutput[10] = static_cast<char>(sizeof(double));

					ConvertFunction();
					if (mPosition != mData.size()) {
						throw std::runtime_error("bytecode has data left over");
					}
					return std::move(mOutput);
				}

			private:
				template<typename T>
				T Read() {
					Require(sizeof(T));

					T value;
					std::memcpy(&value, mData.data() + mPosition, sizeof(T));
					mPosition += sizeof(T);
					return value;
				}

				template<typename T>
				void Write(T value) {
					mOutput.append(reinterpret_cast<const char*>(&value), sizeof(T));
				}

				void Require(size_t count) const {
					if (count > mData.size() - mPosition) {
						throw std::runtime_error("bytecode ended early");
					}
				}

				void Copy(size_t count) {
					Require(count);
					mOutput.append(mData, mPosition, count);
					mPosition += count;
				}

				int32_t CopyCount() {
					const auto count = Read<int32_t>();
					Write(count);
					return count;
				}

				void ConvertString() {
					const auto size = Read<uint32_t>();
					Write(static_cast<size_t>(size));
					Copy(size);
				}

				void ConvertConstants() {
					for (int32_t count = CopyCount(); count > 0; --count) {
						const auto type = static_cast<ConstantType>(Read<uint8_t>());
						Write(type);

						switch (type) {
							case ConstantType::Nil:
								break;

							case ConstantType::Boolean:
								Copy(1);
								break;

							case ConstantType::Number:
								Write(static_cast<double>(Read<float>()));
								break;

							case ConstantType::String:
								ConvertString();
								break;

							default:
								throw std::runtime_error("bytecode has an unknown constant");
						}
					}
				}

				void ConvertDebugInfo() {
					Copy(static_cast<size_t>(CopyCount()) * sizeof(uint32_t)); // line numbers

					for (int32_t count = CopyCount(); count > 0; --count) { // locals
						ConvertString();
						Copy(2 * sizeof(int32_t));
					}

					for (int32_t count = CopyCount(); count > 0; --count) { // upvalues
						ConvertString();
					}
				}

				void ConvertFunction() {
					ConvertString(); // source name
					Copy(2 * sizeof(int32_t)); // first and last line
					Copy(4); // upvalue count, parameter count, vararg flag, stack size
					Copy(static_cast<size_t>(CopyCount()) * sizeof(uint32_t)); // instructions

					ConvertConstants();

					for (int32_t count = CopyCount(); count > 0; --count) {
						ConvertFunction();
					}

					ConvertDebugInfo();
				}

			private:
				const std::string& mData;
				std::string mOutput;
				size_t mPosition = 0;
		};
	}

	// ScriptSource
	std::string ScriptSource::Read(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		std::stringstream buffer;
		buffer << file.rdbuf();

		std::string data = buffer.str();
		return IsGameBytecode(data) ? ConvertBytecode(data) : data;
	}

	bool ScriptSource::IsGameBytecode(const std::string& data) {
		return data.size() >= headerSize && std::memcmp(data.data(), gameHeader, headerSize) == 0;
	}

	std::string ScriptSource::ConvertBytecode(const std::string& data) {
		return BytecodeConverter(data).Convert();
	}
}
