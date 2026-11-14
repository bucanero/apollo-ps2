/*
 * gettimeofday() fix for the pinned ps2sdk
 *
 * That ps2sdk's _gettimeofday (libcglue) returns tv_usec values that aren't
 * microseconds (e.g. 62498304, growing ~25000 per call). libcurl measures its
 * timeouts with gettimeofday(), so it sees seconds pass in a few milliseconds
 * and fails at random with "Timeout was reached".
 *
 * The link wraps gettimeofday (-Wl,--wrap=gettimeofday, see CMakeLists.txt),
 * so every caller (libcurl and libcglue's clock_gettime included) gets this
 * version: the SDK's wall-clock seconds from the first call, plus the elapsed
 * EE bus clock ticks, converted correctly.
 */

#include <sys/time.h>
#include <timer.h>

int __real_gettimeofday(struct timeval *tv, void *tz);

int __wrap_gettimeofday(struct timeval *tv, void *tz)
{
	static time_t base_sec = 0;
	static u64 base_ticks = 0;
	u64 ticks, elapsed;

	if (!tv)
		return __real_gettimeofday(tv, tz);

	ticks = GetTimerSystemTime();

	if (!base_ticks)
	{
		struct timeval now;

		if (__real_gettimeofday(&now, tz) != 0)
			return -1;

		base_sec = now.tv_sec;
		base_ticks = ticks;
	}

	elapsed = ticks - base_ticks;
	tv->tv_sec = base_sec + (time_t)(elapsed / kBUSCLK);
	tv->tv_usec = (suseconds_t)(((elapsed % kBUSCLK) * 1000000ULL) / kBUSCLK);

	return 0;
}
