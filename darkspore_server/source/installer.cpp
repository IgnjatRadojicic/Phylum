//
// Created by vitor on 8/4/25.
//

#include "installer.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

bool Installer::running = false;
std::string Installer::label = "";

void Installer::LoadDarksporeData(std::string darksporeInstallPath, std::string darksporeInstallVersion)
{
	const auto& serverDataVersionBinPath = "data/serverdata/version_bin.txt";
	// Older installs hold decompiled scripts, which have to be replaced by the original ones
	const auto& scriptFormatPath = "data/serverdata/scripts_are_bytecode.txt";
	if (std::filesystem::exists(serverDataVersionBinPath) && std::filesystem::exists(scriptFormatPath)) {
		label = "Server files are ready.";
		std::cout << "Server files are ready." << std::endl;
		return;
	}

	running = true;

#ifdef _WIN32
	std::string dbpf_unpacker = "dbpf_unpacker.exe";
	std::string recap_parser  = "recap_parser.exe";

	// Step 1: Unpack packages
	std::cout << "Unpacking ServerData.package..." << std::endl;
	RunCommand(dbpf_unpacker + " \"" + darksporeInstallPath + R"(\Data\ServerData.package)" + "\" " + R"(ServerData\)");
	std::cout << "Unpacking AssetData_Binary.package..." << std::endl;
	RunCommand(dbpf_unpacker + " \"" + darksporeInstallPath + R"(\Data\AssetData_Binary.package)" + "\" " + R"(AssetData_Binary\)");

	// Step 3: Parse binary assets
	std::cout << "Parsing binary assets from AssetData_Binary..." << std::endl;
	RunCommand(recap_parser + " --recursive --silent --sort-ext --xml -o data\\serverdata AssetData_Binary");
#else
	std::string dbpf_unpacker = "./dbpf_unpacker";
	std::string recap_parser  = "./recap_parser";

	// Step 1: Unpack packages
	label = "Unpacking ServerData.package...";
	std::cout << "Unpacking ServerData.package..." << std::endl;
	RunCommand(dbpf_unpacker + " \"" + darksporeInstallPath + "/Data/ServerData.package\" ./ServerData/");
	label = "Unpacking AssetData_Binary.package...";
	std::cout << "Unpacking AssetData_Binary.package..." << std::endl;
	RunCommand(dbpf_unpacker + " \"" + darksporeInstallPath + "/Data/AssetData_Binary.package\" ./AssetData_Binary/");

	// Step 3: Parse binary assets
	label = "Parsing binary assets from AssetData_Binary...";
	std::cout << "Parsing binary assets from AssetData_Binary..." << std::endl;
	RunCommand(recap_parser + " --recursive --silent --sort-ext --xml -o ./data/serverdata ./AssetData_Binary");
#endif

	// Step 4: Cleanup intermediate directories
	label = "Cleaning temporary folders...";
	std::cout << "Cleaning temporary folders..." << std::endl;
	std::filesystem::remove_all("./AssetData_Binary");

	// Step 5: Move final data into place
	label = "Moving new files to the correct place...";
	CopyScripts("./ServerData", "./data/serverdata");
	std::filesystem::remove_all("./ServerData");

	std::ofstream(serverDataVersionBinPath) << darksporeInstallVersion;
	std::ofstream(scriptFormatPath) << "1";

	running = false;
	label = "Server files are ready.";
	std::cout << "Server files are ready." << std::endl;
}

void Installer::RunCommand(const std::string& cmd) {
	std::filesystem::path log_path = std::filesystem::path("data") / "serverdata" / "debug.log";
	std::string log_cmd = cmd + " 2>&1";
	std::ofstream log(log_path, std::ios::app);
	if (log.is_open()) {
		log << log_cmd << std::endl;
	}

	int result = std::system(log_cmd.c_str());
	if (result != 0) {
		if (log.is_open()) {
			log << "Command failed: " << log_cmd << std::endl;
		}
		exit(result);
	}
}

void Installer::CopyScripts(const std::filesystem::path& source, const std::filesystem::path& destination) {
	for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
		if (entry.is_regular_file() && entry.path().extension() == ".lua") {
			const auto destPath = destination / std::filesystem::relative(entry.path(), source);
			std::filesystem::create_directories(destPath.parent_path());
			std::filesystem::copy_file(entry.path(), destPath, std::filesystem::copy_options::overwrite_existing);
		}
	}
}

void Installer::MergeDirectories(const std::filesystem::path& source, const std::filesystem::path& destination) {
	for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
		if (entry.is_directory()) continue;

		std::filesystem::path relativePath = std::filesystem::relative(entry.path(), source);
		std::filesystem::path destPath = destination / relativePath;

		if (!std::filesystem::exists(destPath)) {
			std::filesystem::create_directories(destPath.parent_path());
			std::filesystem::copy_file(entry.path(), destPath);
		}
	}
}