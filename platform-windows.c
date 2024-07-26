#include "platform.h"

#include "util_uint64.h"

#include <Windows.h>

static bool have_clock_freq = false;
static LARGE_INTEGER clock_freq;

static inline unsigned long long get_clock_freq()
{
	if (!have_clock_freq)
	{
		QueryPerformanceFrequency(&clock_freq);
		have_clock_freq = true;
	}

	return clock_freq.QuadPart;
}

unsigned long long os_gettime_ns()
{
	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);
	return util_mul_div64(current_time.QuadPart, 1000000000ULL, get_clock_freq());
}
bool os_sleep_fastto_ns(unsigned long long target_ns)
{
	unsigned long long current = os_gettime_ns();
	if (target_ns < current)
	{
		return false;
	}

	do
	{
		unsigned long long remain_ms = (target_ns - current) / (1000 * 1000);
		if (!remain_ms)
		{
			remain_ms = 1;
		}

		Sleep((DWORD)remain_ms);

		current = os_gettime_ns();

	} while (target_ns >  current);

	return true;
}