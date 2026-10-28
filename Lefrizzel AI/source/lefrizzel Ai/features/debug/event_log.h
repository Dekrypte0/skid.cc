#pragma once
// Recent combat events for the Debug tab "Events" view.
// In-memory only: no disk I/O on the game thread, no dependency on the log file.
#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace DebugEvents {

constexpr int kCap = 64;
constexpr int kLen = 192;

struct Entry {
	unsigned long long tick = 0;
	char text[kLen]{};
};

inline std::mutex& Mutex()
{
	static std::mutex s_mut;
	return s_mut;
}

inline Entry* Ring()
{
	static Entry s_ring[kCap]{};
	return s_ring;
}

inline int& Head()
{
	static int s_head = 0;
	return s_head;
}

inline int& Count()
{
	static int s_count = 0;
	return s_count;
}

inline void Push(const char* fmt, ...)
{
	if (!fmt || !fmt[0])
		return;
	char body[kLen]{};
	va_list args;
	va_start(args, fmt);
	vsnprintf(body, sizeof(body), fmt, args);
	va_end(args);
	if (!body[0])
		return;

	std::lock_guard<std::mutex> lk(Mutex());
	Entry& e = Ring()[Head()];
	e.tick = GetTickCount64();
	strncpy_s(e.text, body, _TRUNCATE);
	Head() = (Head() + 1) % kCap;
	if (Count() < kCap)
		++Count();
}

// Oldest -> newest, at most maxOut entries. Returns the count written.
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
	const int start = (head - n + kCap * 2) % kCap;
	for (int i = 0; i < n; ++i)
		out[i] = Ring()[(start + i) % kCap];
	return n;
}

inline void Clear()
{
	std::lock_guard<std::mutex> lk(Mutex());
	Head() = 0;
	Count() = 0;
}

} // namespace DebugEvents
