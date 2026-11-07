
// Include
#include "crashreport.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>

#include <cstdio>

namespace {
	void WriteStack(FILE* file, const CONTEXT& crashContext) {
		const HANDLE process = GetCurrentProcess();
		const HANDLE thread = GetCurrentThread();
		SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
		SymInitialize(process, nullptr, TRUE);

		CONTEXT context = crashContext;
		STACKFRAME64 frame {};
		frame.AddrPC.Offset = context.Rip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.Rbp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.Rsp;
		frame.AddrStack.Mode = AddrModeFlat;

		alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256] {};
		auto symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
		for (int depth = 0; depth < 48; ++depth) {
			if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
				SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || frame.AddrPC.Offset == 0) {
				break;
			}

			const DWORD64 address = frame.AddrPC.Offset;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = 255;
			DWORD64 symbolOffset = 0;
			const char* name = SymFromAddr(process, address, &symbolOffset, symbol) ? symbol->Name : "?";

			IMAGEHLP_LINE64 line {};
			line.SizeOfStruct = sizeof(line);
			DWORD lineOffset = 0;
			if (SymGetLineFromAddr64(process, address, &lineOffset, &line)) {
				std::fprintf(file, "  %s  %s:%lu\n", name, line.FileName, line.LineNumber);
			} else {
				std::fprintf(file, "  %s +0x%llx\n", name, static_cast<unsigned long long>(symbolOffset));
			}
		}
		SymCleanup(process);
	}

	void WriteDump(EXCEPTION_POINTERS* exception) {
		const HANDLE dump = CreateFileA("crash.dmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (dump == INVALID_HANDLE_VALUE) {
			return;
		}

		MINIDUMP_EXCEPTION_INFORMATION info { GetCurrentThreadId(), exception, FALSE };
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump, MiniDumpNormal, &info, nullptr, nullptr);
		CloseHandle(dump);
	}

	LONG WINAPI OnCrash(EXCEPTION_POINTERS* exception) {
		if (FILE* file = std::fopen("crash.txt", "w")) {
			const auto& record = *exception->ExceptionRecord;
			std::fprintf(file, "exception 0x%08lx at %p\n", record.ExceptionCode, record.ExceptionAddress);
			if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
				std::fprintf(file, "%s address 0x%llx\n", record.ExceptionInformation[0] ? "write" : "read",
					static_cast<unsigned long long>(record.ExceptionInformation[1]));
			}
			WriteStack(file, *exception->ContextRecord);
			std::fclose(file);
		}
		WriteDump(exception);
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void CrashReport::Install() {
	SetUnhandledExceptionFilter(OnCrash);
}
#else
void CrashReport::Install() {}
#endif
