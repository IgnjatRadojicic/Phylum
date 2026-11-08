
#ifndef _CRASH_REPORT_HEADER
#define _CRASH_REPORT_HEADER

// CrashReport
// On a crash the server writes crash.txt (exception and call stack with file:line) and crash.dmp
// next to the working directory, so a crash seen in game can be traced without a debugger.
class CrashReport {
	public:
		static void Install();

		// For the calling thread: a report on an uncaught C++ exception, and stack room so a stack
		// overflow there still gets one.
		static void GuardThread();
};

#endif
