#pragma once
// Header-only file sink so Release diagnostics survive (console.cpp is _DEBUG-only).
// Every init/feature diagnostic line lands in %TEMP%\skidcc_log.txt.
//
// Keeps the last kRingCap lines in memory as well so the in-menu Debug tab can
// mirror the console without re-reading the file every frame (the file is
// opened/closed per line and would stall Present).
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <mutex>

namespace FileLog {

constexpr int kRingCap = 400;
constexpr int kLineCap = 240;

struct Entry {
	unsigned long long tick; // GetTickCount64 at write time
	char text[kLineCap];     // "[hh:mm:ss.mmm] body"
};

// Function-local statics inside inline functions are one instance per process.
inline std::mutex& Mutex()
{
	static std::mutex s_mut;
	return s_mut;
}

// Separate lock for the disk sink. A writer holds it across fopen/fprintf/fclose
// (milliseconds); Snapshot() must never wait on that or the Debug console tab
// would stall Present every frame.
inline std::mutex& FileMutex()
{
	static std::mutex s_mut;
	return s_mut;
}

inline Entry* Ring()
{
	static Entry s_ring[kRingCap]{};
	return s_ring;
}

inline int& Head() // next write slot
{
	static int s_head = 0;
	return s_head;
}

inline int& Count()
{
	static int s_count = 0;
	return s_count;
}

inline unsigned long long& Total()
{
	static unsigned long long s_total = 0;
	return s_total;
}

// Call once at boot before any logging - clears the previous session's file.
inline void Reset()
{
	char path[MAX_PATH]{};
	if (!GetTempPathA(MAX_PATH, path))
		return;
	if (strcat_s(path, "skidcc_log.txt") != 0)
		return;
	DeleteFileA(path);
}

// Caller must hold Mutex().
inline void PushRingLocked(const char* line)
{
	if (!line || !line[0])
		return;
	Entry& e = Ring()[Head()];
	e.tick = GetTickCount64();
	strncpy_s(e.text, line, _TRUNCATE);
	Head() = (Head() + 1) % kRingCap;
	if (Count() < kRingCap)
		++Count();
	++Total();
}

inline void PushRing(const char* line)
{
	if (!line || !line[0])
		return;
	std::lock_guard<std::mutex> lk(Mutex());
	PushRingLocked(line);
}

inline void VLine(const char* fmt, va_list args)
{
	// Only the disk sink can hard-fail (no %TEMP% / quota). The in-memory ring
	// must keep working so the Debug tab always shows what the cheat is doing.
	static volatile long s_fileFailed = 0;

	char body[1024]{};
	if (fmt && fmt[0]) {
		vsnprintf(body, sizeof(body), fmt, args);
	} else {
		body[0] = '\0';
	}
	if (!body[0])
		return;

	SYSTEMTIME st{};
	GetLocalTime(&st);

	// Ring first: the Debug tab must keep working even if the disk write fails.
	char line[kLineCap]{};
	_snprintf_s(line, sizeof(line), _TRUNCATE, "[%02u:%02u:%02u.%03u] %s",
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, body);

	// Ring first: the Debug tab must keep working even if the disk write fails.
	{
		std::lock_guard<std::mutex> lk(Mutex());
		PushRingLocked(line);
	}

	if (InterlockedExchangeAdd(&s_fileFailed, 0))
		return;

	char path[MAX_PATH]{};
	if (!GetTempPathA(MAX_PATH, path) || strcat_s(path, "skidcc_log.txt") != 0) {
		InterlockedExchange(&s_fileFailed, 1);
		return;
	}
	// File lock only: Snapshot() can still read the ring while a slow disk
	// write is in flight.
	std::lock_guard<std::mutex> fk(FileMutex());
	FILE* f = nullptr;
	if (fopen_s(&f, path, "ab") != 0 || !f) {
		InterlockedExchange(&s_fileFailed, 1);
		return;
	}
	fprintf(f, "%s\n", line);
	fclose(f);
}

inline void Line(const char* fmt, ...)
{
	va_list a;
	va_start(a, fmt);
	VLine(fmt, a);
	va_end(a);
}

// -- in-menu console -------------------------------------------------------

// Copies oldest -> newest, at most maxOut entries. Returns the count written.
inline int Snapshot(Entry* out, int maxOut)
{
	if (!out || maxOut <= 0)
		return 0;
	std::lock_guard<std::mutex> lk(Mutex());
	const int have = Count();
	const int n = (have < maxOut) ? have : maxOut;
	if (n <= 0)
		return 0;
	const int head = Head();
	const int start = (head - n + kRingCap * 2) % kRingCap;
	for (int i = 0; i < n; ++i)
		out[i] = Ring()[(start + i) % kRingCap];
	return n;
}

inline int SnapshotCount()
{
	std::lock_guard<std::mutex> lk(Mutex());
	return Count();
}

// Monotonic counter of lines ever written - lets the viewer detect "new" even
// when the ring wrapped.
inline unsigned long long TotalLines()
{
	std::lock_guard<std::mutex> lk(Mutex());
	return Total();
}

inline void ClearRing()
{
	std::lock_guard<std::mutex> lk(Mutex());
	Head() = 0;
	Count() = 0;
	Total() = 0;
}

inline bool TempLogPath(char* out, size_t outSize)
{
	if (!out || outSize < 16)
		return false;
	char path[MAX_PATH]{};
	if (!GetTempPathA(MAX_PATH, path))
		return false;
	if (strcat_s(path, "skidcc_log.txt") != 0)
		return false;
	strncpy_s(out, outSize, path, _TRUNCATE);
	return true;
}

} // namespace FileLog
