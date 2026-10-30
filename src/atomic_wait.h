/* SPDX-License-Identifier: MIT */
#ifndef RINICU_ATOMIC_WAIT_H
#define RINICU_ATOMIC_WAIT_H

#include <errno.h>
#include <stdint.h>
#include <time.h>

#if defined(__rinos__)
#include "../../libc/rin_atomic_wait.h"
#elif defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#else
#include <sched.h>
#endif

/* Keep the client independent from pthread storage.  The public client has
 * only four uintptr_t words, so a naturally aligned u32 is the wait key and
 * the kernel provides the sleep/wake queue. */
static inline int rin_icu_atomic_wait_u32(volatile uint32_t* address,
                                          uint32_t expected,
                                          const struct timespec* timeout)
{
    if (!address) {
        errno = EINVAL;
        return -1;
    }
#if defined(__rinos__)
    return rin_atomic_wait_u32(address, expected, timeout);
#elif defined(__linux__)
    {
        long result = syscall(SYS_futex, address, FUTEX_WAIT_PRIVATE,
                              (int32_t)expected, timeout, NULL, 0);
        if (result == 0 || errno == EAGAIN || errno == EINTR) return 0;
        return -1;
    }
#else
    if (timeout) {
        struct timespec remaining = *timeout;
        while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
        }
    } else {
        sched_yield();
    }
    return 0;
#endif
}

static inline int rin_icu_atomic_notify_all_u32(volatile uint32_t* address)
{
    if (!address) {
        errno = EINVAL;
        return -1;
    }
#if defined(__rinos__)
    return rin_atomic_notify_all_u32(address);
#elif defined(__linux__)
    {
        long result = syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE,
                              0x7fffffff, NULL, NULL, 0);
        return result < 0 ? -1 : (int)result;
    }
#else
    (void)address;
    return 0;
#endif
}

#endif /* RINICU_ATOMIC_WAIT_H */
