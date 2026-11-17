/*
 * Workarounds for the pinned ps2sdk
 *
 * The link wraps the functions below (-Wl,--wrap=..., see CMakeLists.txt), so
 * every caller, libcurl, wolfSSL and the SDK's own libraries included, gets
 * the fixed versions here instead of the SDK's.
 *
 *  - gettimeofday(): microseconds that aren't microseconds
 *  - lwIP sockets:   errno never set on failure
 */

#include <errno.h>
#include <sys/time.h>
#include <timer.h>
#include <ps2ip.h>

/*
 * gettimeofday()
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

/*
 * errno for lwIP sockets
 *
 * libcurl and wolfSSL call lwIP's socket functions directly (lwip_connect,
 * lwip_recv, ...) and read errno to tell "in progress / try again" from real
 * errors. This ps2sdk's EE lwIP never sets errno: it keeps the error on the
 * socket (SO_ERROR). With non-blocking sockets curl then sees errno 0 and
 * drops every connection ("Immediate connect fail ... Success").
 *
 * The link wraps these calls (-Wl,--wrap=..., see CMakeLists.txt) and sets
 * errno on failure: the socket's pending error if there is one, otherwise the
 * non-blocking "not ready yet" code. lwIP's error numbers are Linux-style, so
 * the ones curl and wolfSSL check are translated to newlib's.
 */

int __real_lwip_connect(int s, const struct sockaddr *name, socklen_t namelen);
int __real_lwip_recv(int s, void *mem, size_t len, int flags);
int __real_lwip_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int __real_lwip_send(int s, const void *dataptr, size_t size, int flags);
int __real_lwip_sendto(int s, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);

// lwIP (Linux numbering) -> newlib
static int lwip_to_newlib_errno(int err)
{
	switch (err)
	{
	case 11:  return EAGAIN;
	case 115: return EINPROGRESS;
	case 114: return EALREADY;
	case 106: return EISCONN;
	case 107: return ENOTCONN;
	case 110: return ETIMEDOUT;
	case 111: return ECONNREFUSED;
	case 104: return ECONNRESET;
	case 103: return ECONNABORTED;
	case 113: return EHOSTUNREACH;
	case 101: return ENETUNREACH;
	case 98:  return EADDRINUSE;
	case 9:   return EBADF;
	case 12:  return ENOMEM;
	case 22:  return EINVAL;
	default:  return err;
	}
}

static int set_socket_errno(int s, int would_block)
{
	int err = 0;
	socklen_t len = sizeof(err);

	if (lwip_getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err == 0)
		err = would_block;
	else
		err = lwip_to_newlib_errno(err);

	errno = err;
	return -1;
}

int __wrap_lwip_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
	int ret = __real_lwip_connect(s, name, namelen);

	return (ret < 0) ? set_socket_errno(s, EINPROGRESS) : ret;
}

int __wrap_lwip_recv(int s, void *mem, size_t len, int flags)
{
	int ret = __real_lwip_recv(s, mem, len, flags);

	return (ret < 0) ? set_socket_errno(s, EAGAIN) : ret;
}

int __wrap_lwip_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	int ret = __real_lwip_recvfrom(s, mem, len, flags, from, fromlen);

	return (ret < 0) ? set_socket_errno(s, EAGAIN) : ret;
}

int __wrap_lwip_send(int s, const void *dataptr, size_t size, int flags)
{
	int ret = __real_lwip_send(s, dataptr, size, flags);

	return (ret < 0) ? set_socket_errno(s, EAGAIN) : ret;
}

int __wrap_lwip_sendto(int s, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen)
{
	int ret = __real_lwip_sendto(s, dataptr, size, flags, to, tolen);

	return (ret < 0) ? set_socket_errno(s, EAGAIN) : ret;
}
