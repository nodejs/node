/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>

#define USE_SOCKETS
#include "e_os.h"
#include "internal/cryptlib.h"
#include <openssl/rand.h>
#include "rand_lcl.h"

#if !(defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VMS) || defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_UEFI))

# include <sys/types.h>
# include <sys/time.h>
# include <sys/times.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <time.h>
# if defined(OPENSSL_SYS_LINUX) /* should actually be available virtually
                                 * everywhere */
#  include <poll.h>
# endif
# include <limits.h>
# ifndef FD_SETSIZE
#  define FD_SETSIZE (8*sizeof(fd_set))
# endif

# if defined(OPENSSL_SYS_VOS)

/*
 * The following algorithm repeatedly samples the real-time clock (RTC) to
 * generate a sequence of unpredictable data.  The algorithm relies upon the
 * uneven execution speed of the code (due to factors such as cache misses,
 * interrupts, bus activity, and scheduling) and upon the rather large
 * relative difference between the speed of the clock and the rate at which
 * it can be read.
 *
 * If this code is ported to an environment where execution speed is more
 * constant or where the RTC ticks at a much slower rate, or the clock can be
 * read with fewer instructions, it is likely that the results would be far
 * more predictable.
 *
 * As a precaution, we generate 4 times the minimum required amount of seed
 * data.
 */

int RAND_poll(void)
{
    short int code;
    gid_t curr_gid;
    pid_t curr_pid;
    uid_t curr_uid;
    int i, k;
    struct timespec ts;
    unsigned char v;

#  ifdef OPENSSL_SYS_VOS_HPPA
    long duration;
    extern void s$sleep(long *_duration, short int *_code);
#  else
#   ifdef OPENSSL_SYS_VOS_IA32
    long long duration;
    extern void s$sleep2(long long *_duration, short int *_code);
#   else
#    error "Unsupported Platform."
#   endif                       /* OPENSSL_SYS_VOS_IA32 */
#  endif                        /* OPENSSL_SYS_VOS_HPPA */

    /*
     * Seed with the gid, pid, and uid, to ensure *some* variation between
     * different processes.
     */

    curr_gid = getgid();
    RAND_add(&curr_gid, sizeof(curr_gid), 1);
    curr_gid = 0;

    curr_pid = getpid();
    RAND_add(&curr_pid, sizeof(curr_pid), 1);
    curr_pid = 0;

    curr_uid = getuid();
    RAND_add(&curr_uid, sizeof(curr_uid), 1);
    curr_uid = 0;

    for (i = 0; i < (ENTROPY_NEEDED * 4); i++) {
        /*
         * burn some cpu; hope for interrupts, cache collisions, bus
         * interference, etc.
         */
        for (k = 0; k < 99; k++)
            ts.tv_nsec = random();

#  ifdef OPENSSL_SYS_VOS_HPPA
        /* sleep for 1/1024 of a second (976 us).  */
        duration = 1;
        s$sleep(&duration, &code);
#  else
#   ifdef OPENSSL_SYS_VOS_IA32
        /* sleep for 1/65536 of a second (15 us).  */
        duration = 1;
        s$sleep2(&duration, &code);
#   endif                       /* OPENSSL_SYS_VOS_IA32 */
#  endif                        /* OPENSSL_SYS_VOS_HPPA */

        /* get wall clock time.  */
        clock_gettime(CLOCK_REALTIME, &ts);

        /* take 8 bits */
        v = (unsigned char)(ts.tv_nsec % 256);
        RAND_add(&v, sizeof(v), 1);
        v = 0;
    }
    return 1;
}
# elif defined __OpenBSD__
int RAND_poll(void)
{
    u_int32_t rnd = 0, i;
    unsigned char buf[ENTROPY_NEEDED];

    for (i = 0; i < sizeof(buf); i++) {
        if (i % 4 == 0)
            rnd = arc4random();
        buf[i] = rnd;
        rnd >>= 8;
    }
    RAND_add(buf, sizeof(buf), ENTROPY_NEEDED);
    OPENSSL_cleanse(buf, sizeof(buf));

    return 1;
}
# else                          /* !defined(__OpenBSD__) */
int RAND_poll(void)
{
    unsigned long l;
    pid_t curr_pid = getpid();
#  if defined(DEVRANDOM) || (!defined(OPENSS_NO_EGD) && defined(DEVRANDOM_EGD))
    unsigned char tmpbuf[ENTROPY_NEEDED];
    int n = 0;
#  endif
#  ifdef DEVRANDOM
    static const char *randomfiles[] = { DEVRANDOM };
    struct stat randomstats[OSSL_NELEM(randomfiles)];
    int fd;
    unsigned int i;
#  endif
#  if !defined(OPENSSL_NO_EGD) && defined(DEVRANDOM_EGD)
    static const char *egdsockets[] = { DEVRANDOM_EGD, NULL };
    const char **egdsocket = NULL;
#  endif

#  ifdef DEVRANDOM
    memset(randomstats, 0, sizeof(randomstats));
    /*
     * Use a random entropy pool device. Linux, FreeBSD and OpenBSD have
     * this. Use /dev/urandom if you can as /dev/random may block if it runs
     * out of random entries.
     */

    for (i = 0; (i < OSSL_NELEM(randomfiles)) && (n < ENTROPY_NEEDED); i++) {
        if ((fd = open(randomfiles[i], O_RDONLY
#   ifdef O_NONBLOCK
                       | O_NONBLOCK
#   endif
#   ifdef O_BINARY
                       | O_BINARY
#   endif
#   ifdef O_NOCTTY              /* If it happens to be a TTY (god forbid), do
                                 * not make it our controlling tty */
                       | O_NOCTTY
#   endif
             )) >= 0) {
            int usec = 10 * 1000; /* spend 10ms on each file */
            int r;
            unsigned int j;
            struct stat *st = &randomstats[i];

            /*
             * Avoid using same input... Used to be O_NOFOLLOW above, but
             * it's not universally appropriate...
             */
            if (fstat(fd, st) != 0) {
                close(fd);
                continue;
            }
            for (j = 0; j < i; j++) {
                if (randomstats[j].st_ino == st->st_ino &&
                    randomstats[j].st_dev == st->st_dev)
                    break;
            }
            if (j < i) {
                close(fd);
                continue;
            }

            do {
                int try_read = 0;

#   if defined(OPENSSL_SYS_LINUX)
                /* use poll() */
                struct pollfd pset;

                pset.fd = fd;
                pset.events = POLLIN;
                pset.revents = 0;

                if (poll(&pset, 1, usec / 1000) < 0)
                    usec = 0;
                else
                    try_read = (pset.revents & POLLIN) != 0;

#   else
                /* use select() */
                fd_set fset;
                struct timeval t;

                t.tv_sec = 0;
                t.tv_usec = usec;

                if (FD_SETSIZE > 0 && (unsigned)fd >= FD_SETSIZE) {
                    /*
                     * can't use select, so just try to read once anyway
                     */
                    try_read = 1;
                } else {
                    FD_ZERO(&fset);
                    FD_SET(fd, &fset);

                    if (select(fd + 1, &fset, NULL, NULL, &t) >= 0) {
                        usec = t.tv_usec;
                        if (FD_ISSET(fd, &fset))
                            try_read = 1;
                    } else
                        usec = 0;
                }
#   endif

                if (try_read) {
                    r = read(fd, (unsigned char *)tmpbuf + n,
                             ENTROPY_NEEDED - n);
                    if (r > 0)
                        n += r;
                } else
                    r = -1;

                /*
                 * Some Unixen will update t in select(), some won't.  For
                 * those who won't, or if we didn't use select() in the first
                 * place, give up here, otherwise, we will do this once again
                 * for the remaining time.
                 */
                if (usec == 10 * 1000)
                    usec = 0;
            }
            while ((r > 0 ||
                    (errno == EINTR || errno == EAGAIN)) && usec != 0
                   && n < ENTROPY_NEEDED);

            close(fd);
        }
    }
#  endif                        /* defined(DEVRANDOM) */

#  if !defined(OPENSSL_NO_EGD) && defined(DEVRANDOM_EGD)
    /*
     * Use an EGD socket to read entropy from an EGD or PRNGD entropy
     * collecting daemon.
     */

    for (egdsocket = egdsockets; *egdsocket && n < ENTROPY_NEEDED;
         egdsocket++) {
        int r;

        r = RAND_query_egd_bytes(*egdsocket, (unsigned char *)tmpbuf + n,
                                 ENTROPY_NEEDED - n);
        if (r > 0)
            n += r;
    }
#  endif                        /* defined(DEVRANDOM_EGD) */

#  if defined(DEVRANDOM) || (!defined(OPENSSL_NO_EGD) && defined(DEVRANDOM_EGD))
    if (n > 0) {
        RAND_add(tmpbuf, sizeof(tmpbuf), (double)n);
        OPENSSL_cleanse(tmpbuf, n);
    }
#  endif

    /* put in some default random data, we need more than just this */
    l = curr_pid;
    RAND_add(&l, sizeof(l), 0.0);
    l = getuid();
    RAND_add(&l, sizeof(l), 0.0);

    l = time(NULL);
    RAND_add(&l, sizeof(l), 0.0);

#  if defined(DEVRANDOM) || (!defined(OPENSSL_NO_EGD) && defined(DEVRANDOM_EGD))
    return 1;
#  else
    return 0;
#  endif
}

# endif                         /* defined(__OpenBSD__) */
#endif                          /* !(defined(OPENSSL_SYS_WINDOWS) ||
                                 * defined(OPENSSL_SYS_WIN32) ||
                                 * defined(OPENSSL_SYS_VMS) ||
                                 * defined(OPENSSL_SYS_VXWORKS) */

#if defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_UEFI)
int RAND_poll(void)
{
    return 0;
}
#endif
