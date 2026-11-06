
// Include
#include "scriptsource.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>

// Game
namespace Game {
	namespace {
		// A decompiled local: L<register>_<unique>
		struct Local {
			int reg = 0;
			int unique = 0;
			size_t end = 0;
		};

		std::optional<Local> ReadLocal(const std::string& line, size_t start) {
			if (start >= line.size() || line[start] != 'L') {
				return std::nullopt;
			}

			size_t position = start + 1;
			auto readNumber = [&line, &position]() -> std::optional<int> {
				const size_t first = position;
				int value = 0;
				while (position < line.size() && std::isdigit(static_cast<unsigned char>(line[position]))) {
					value = value * 10 + (line[position++] - '0');
				}
				return position == first ? std::nullopt : std::optional<int>(value);
			};

			const auto reg = readNumber();
			if (!reg || position >= line.size() || line[position++] != '_') {
				return std::nullopt;
			}

			const auto unique = readNumber();
			if (!unique) {
				return std::nullopt;
			}
			return Local { *reg, *unique, position };
		}

		std::string RestoreLine(const std::string& line) {
			const size_t start = line.find_first_not_of(" \t");
			if (start == std::string::npos) {
				return line;
			}

			const auto target = ReadLocal(line, start);
			if (!target || line.compare(target->end, 3, " = ") != 0) {
				return line;
			}

			const auto function = ReadLocal(line, target->end + 3);
			if (!function || function->end >= line.size() || line[function->end] != '(') {
				return line;
			}

			// Registers in a call's results are consecutive and declared together.
			const int count = target->reg - function->reg;
			if (count <= 0 || target->unique - function->unique != count) {
				return line;
			}

			std::string result = line.substr(0, start);
			for (int i = 0; i <= count; ++i) {
				char name[32];
				std::snprintf(name, sizeof(name), "%sL%d_%d", i == 0 ? "" : ", ", function->reg + i, function->unique + i);
				result += name;
			}
			return result + line.substr(target->end);
		}
	}

	// ScriptSource
	std::string ScriptSource::Read(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		std::stringstream buffer;
		buffer << file.rdbuf();
		return RestoreMultipleAssignments(buffer.str());
	}

	std::string ScriptSource::RestoreMultipleAssignments(const std::string& source) {
		std::string result;
		result.reserve(source.size());

		size_t start = 0;
		while (start < source.size()) {
			size_t end = source.find('\n', start);
			if (end == std::string::npos) {
				end = source.size();
			}

			result += RestoreLine(source.substr(start, end - start));
			if (end < source.size()) {
				result += '\n';
			}
			start = end + 1;
		}
		return result;
	}
}
