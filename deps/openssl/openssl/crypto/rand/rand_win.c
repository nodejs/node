/* crypto/rand/rand_win.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include "cryptlib.h"
#include <openssl/rand.h>
#include "rand_lcl.h"

#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_WIN32)
# include <windows.h>
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0400
# endif
# include <wincrypt.h>
# include <tlhelp32.h>

/*
 * Limit the time spent walking through the heap, processes, threads and
 * modules to a maximum of 1000 miliseconds each, unless CryptoGenRandom
 * failed
 */
# define MAXDELAY 1000

/*
 * Intel hardware RNG CSP -- available from
 * http://developer.intel.com/design/security/rng/redist_license.htm
 */
# define PROV_INTEL_SEC 22
# define INTEL_DEF_PROV L"Intel Hardware Cryptographic Service Provider"

static void readtimer(void);
static void readscreen(void);

/*
 * It appears like CURSORINFO, PCURSORINFO and LPCURSORINFO are only defined
 * when WINVER is 0x0500 and up, which currently only happens on Win2000.
 * Unfortunately, those are typedefs, so they're a little bit difficult to
 * detect properly.  On the other hand, the macro CURSOR_SHOWING is defined
 * within the same conditional, so it can be use to detect the absence of
 * said typedefs.
 */

# ifndef CURSOR_SHOWING
/*
 * Information about the global cursor.
 */
typedef struct tagCURSORINFO {
    DWORD cbSize;
    DWORD flags;
    HCURSOR hCursor;
    POINT ptScreenPos;
} CURSORINFO, *PCURSORINFO, *LPCURSORINFO;

#  define CURSOR_SHOWING     0x00000001
# endif                         /* CURSOR_SHOWING */

# if !defined(OPENSSL_SYS_WINCE)
typedef BOOL(WINAPI *CRYPTACQUIRECONTEXTW) (HCRYPTPROV *, LPCWSTR, LPCWSTR,
                                            DWORD, DWORD);
typedef BOOL(WINAPI *CRYPTGENRANDOM) (HCRYPTPROV, DWORD, BYTE *);
typedef BOOL(WINAPI *CRYPTRELEASECONTEXT) (HCRYPTPROV, DWORD);

typedef HWND(WINAPI *GETFOREGROUNDWINDOW) (VOID);
typedef BOOL(WINAPI *GETCURSORINFO) (PCURSORINFO);
typedef DWORD(WINAPI *GETQUEUESTATUS) (UINT);

typedef HANDLE(WINAPI *CREATETOOLHELP32SNAPSHOT) (DWORD, DWORD);
typedef BOOL(WINAPI *CLOSETOOLHELP32SNAPSHOT) (HANDLE);
typedef BOOL(WINAPI *HEAP32FIRST) (LPHEAPENTRY32, DWORD, size_t);
typedef BOOL(WINAPI *HEAP32NEXT) (LPHEAPENTRY32);
typedef BOOL(WINAPI *HEAP32LIST) (HANDLE, LPHEAPLIST32);
typedef BOOL(WINAPI *PROCESS32) (HANDLE, LPPROCESSENTRY32);
typedef BOOL(WINAPI *THREAD32) (HANDLE, LPTHREADENTRY32);
typedef BOOL(WINAPI *MODULE32) (HANDLE, LPMODULEENTRY32);

#  include <lmcons.h>
#  include <lmstats.h>
#  if 1
/*
 * The NET API is Unicode only.  It requires the use of the UNICODE macro.
 * When UNICODE is defined LPTSTR becomes LPWSTR.  LMSTR was was added to the
 * Platform SDK to allow the NET API to be used in non-Unicode applications
 * provided that Unicode strings were still used for input.  LMSTR is defined
 * as LPWSTR.
 */
typedef NET_API_STATUS(NET_API_FUNCTION *NETSTATGET)
 (LPWSTR, LPWSTR, DWORD, DWORD, LPBYTE *);
typedef NET_API_STATUS(NET_API_FUNCTION *NETFREE) (LPBYTE);
#  endif                        /* 1 */
# endif                         /* !OPENSSL_SYS_WINCE */

#define NOTTOOLONG(start) ((GetTickCount() - (start)) < MAXDELAY)

int RAND_poll(void)
{
    MEMORYSTATUS m;
    HCRYPTPROV hProvider = 0;
    DWORD w;
    int good = 0;

# if defined(OPENSSL_SYS_WINCE)
#  if defined(_WIN32_WCE) && _WIN32_WCE>=300
    /*
     * Even though MSDN says _WIN32_WCE>=210, it doesn't seem to be available
     * in commonly available implementations prior 300...
     */
    {
        BYTE buf[64];
        /* poll the CryptoAPI PRNG */
        /* The CryptoAPI returns sizeof(buf) bytes of randomness */
        if (CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL,
                                 CRYPT_VERIFYCONTEXT)) {
            if (CryptGenRandom(hProvider, sizeof(buf), buf))
                RAND_add(buf, sizeof(buf), sizeof(buf));
            CryptReleaseContext(hProvider, 0);
        }
    }
#  endif
# else                          /* OPENSSL_SYS_WINCE */
    /*
     * None of below libraries are present on Windows CE, which is
     * why we #ifndef the whole section. This also excuses us from
     * handling the GetProcAddress issue. The trouble is that in
     * real Win32 API GetProcAddress is available in ANSI flavor
     * only. In WinCE on the other hand GetProcAddress is a macro
     * most commonly defined as GetProcAddressW, which accepts
     * Unicode argument. If we were to call GetProcAddress under
     * WinCE, I'd recommend to either redefine GetProcAddress as
     * GetProcAddressA (there seem to be one in common CE spec) or
     * implement own shim routine, which would accept ANSI argument
     * and expand it to Unicode.
     */
    {
        /* load functions dynamically - not available on all systems */
        HMODULE advapi = LoadLibrary(TEXT("ADVAPI32.DLL"));
        HMODULE kernel = LoadLibrary(TEXT("KERNEL32.DLL"));
        HMODULE user = NULL;
        HMODULE netapi = LoadLibrary(TEXT("NETAPI32.DLL"));
        CRYPTACQUIRECONTEXTW acquire = NULL;
        CRYPTGENRANDOM gen = NULL;
        CRYPTRELEASECONTEXT release = NULL;
        NETSTATGET netstatget = NULL;
        NETFREE netfree = NULL;
        BYTE buf[64];

        if (netapi) {
            netstatget =
                (NETSTATGET) GetProcAddress(netapi, "NetStatisticsGet");
            netfree = (NETFREE) GetProcAddress(netapi, "NetApiBufferFree");
        }

        if (netstatget && netfree) {
            LPBYTE outbuf;
            /*
             * NetStatisticsGet() is a Unicode only function
             * STAT_WORKSTATION_0 contains 45 fields and STAT_SERVER_0
             * contains 17 fields.  We treat each field as a source of one
             * byte of entropy.
             */

            if (netstatget(NULL, L"LanmanWorkstation", 0, 0, &outbuf) == 0) {
                RAND_add(outbuf, sizeof(STAT_WORKSTATION_0), 45);
                netfree(outbuf);
            }
            if (netstatget(NULL, L"LanmanServer", 0, 0, &outbuf) == 0) {
                RAND_add(outbuf, sizeof(STAT_SERVER_0), 17);
                netfree(outbuf);
            }
        }

        if (netapi)
            FreeLibrary(netapi);

        /*
         * It appears like this can cause an exception deep within
         * ADVAPI32.DLL at random times on Windows 2000.  Reported by Jeffrey
         * Altman. Only use it on NT.
         */

        if (advapi) {
            /*
             * If it's available, then it's available in both ANSI
             * and UNICODE flavors even in Win9x, documentation says.
             * We favor Unicode...
             */
            acquire = (CRYPTACQUIRECONTEXTW) GetProcAddress(advapi,
                                                            "CryptAcquireContextW");
            gen = (CRYPTGENRANDOM) GetProcAddress(advapi, "CryptGenRandom");
            release = (CRYPTRELEASECONTEXT) GetProcAddress(advapi,
                                                           "CryptReleaseContext");
        }

        if (acquire && gen && release) {
            /* poll the CryptoAPI PRNG */
            /* The CryptoAPI returns sizeof(buf) bytes of randomness */
            if (acquire(&hProvider, NULL, NULL, PROV_RSA_FULL,
                        CRYPT_VERIFYCONTEXT)) {
                if (gen(hProvider, sizeof(buf), buf) != 0) {
                    RAND_add(buf, sizeof(buf), 0);
                    good = 1;
#  if 0
                    printf("randomness from PROV_RSA_FULL\n");
#  endif
                }
                release(hProvider, 0);
            }

            /* poll the Pentium PRG with CryptoAPI */
            if (acquire(&hProvider, 0, INTEL_DEF_PROV, PROV_INTEL_SEC, 0)) {
                if (gen(hProvider, sizeof(buf), buf) != 0) {
                    RAND_add(buf, sizeof(buf), sizeof(buf));
                    good = 1;
#  if 0
                    printf("randomness from PROV_INTEL_SEC\n");
#  endif
                }
                release(hProvider, 0);
            }
        }

        if (advapi)
            FreeLibrary(advapi);

        if ((!check_winnt() ||
             !OPENSSL_isservice()) &&
            (user = LoadLibrary(TEXT("USER32.DLL")))) {
            GETCURSORINFO cursor;
            GETFOREGROUNDWINDOW win;
            GETQUEUESTATUS queue;

            win =
                (GETFOREGROUNDWINDOW) GetProcAddress(user,
                                                     "GetForegroundWindow");
            cursor = (GETCURSORINFO) GetProcAddress(user, "GetCursorInfo");
            queue = (GETQUEUESTATUS) GetProcAddress(user, "GetQueueStatus");

            if (win) {
                /* window handle */
                HWND h = win();
                RAND_add(&h, sizeof(h), 0);
            }
            if (cursor) {
                /*
                 * unfortunately, its not safe to call GetCursorInfo() on NT4
                 * even though it exists in SP3 (or SP6) and higher.
                 */
                if (check_winnt() && !check_win_minplat(5))
                    cursor = 0;
            }
            if (cursor) {
                /* cursor position */
                /* assume 2 bytes of entropy */
                CURSORINFO ci;
                ci.cbSize = sizeof(CURSORINFO);
                if (cursor(&ci))
                    RAND_add(&ci, ci.cbSize, 2);
            }

            if (queue) {
                /* message queue status */
                /* assume 1 byte of entropy */
                w = queue(QS_ALLEVENTS);
                RAND_add(&w, sizeof(w), 1);
            }

            FreeLibrary(user);
        }

        /*-
         * Toolhelp32 snapshot: enumerate processes, threads, modules and heap
         * http://msdn.microsoft.com/library/psdk/winbase/toolhelp_5pfd.htm
         * (Win 9x and 2000 only, not available on NT)
         *
         * This seeding method was proposed in Peter Gutmann, Software
         * Generation of Practically Strong Random Numbers,
         * http://www.usenix.org/publications/library/proceedings/sec98/gutmann.html
         * revised version at http://www.cryptoengines.com/~peter/06_random.pdf
         * (The assignment of entropy estimates below is arbitrary, but based
         * on Peter's analysis the full poll appears to be safe. Additional
         * interactive seeding is encouraged.)
         */

        if (kernel) {
            CREATETOOLHELP32SNAPSHOT snap;
            CLOSETOOLHELP32SNAPSHOT close_snap;
            HANDLE handle;

            HEAP32FIRST heap_first;
            HEAP32NEXT heap_next;
            HEAP32LIST heaplist_first, heaplist_next;
            PROCESS32 process_first, process_next;
            THREAD32 thread_first, thread_next;
            MODULE32 module_first, module_next;

            HEAPLIST32 hlist;
            HEAPENTRY32 hentry;
            PROCESSENTRY32 p;
            THREADENTRY32 t;
            MODULEENTRY32 m;
            DWORD starttime = 0;

            snap = (CREATETOOLHELP32SNAPSHOT)
                GetProcAddress(kernel, "CreateToolhelp32Snapshot");
            close_snap = (CLOSETOOLHELP32SNAPSHOT)
                GetProcAddress(kernel, "CloseToolhelp32Snapshot");
            heap_first = (HEAP32FIRST) GetProcAddress(kernel, "Heap32First");
            heap_next = (HEAP32NEXT) GetProcAddress(kernel, "Heap32Next");
            heaplist_first =
                (HEAP32LIST) GetProcAddress(kernel, "Heap32ListFirst");
            heaplist_next =
                (HEAP32LIST) GetProcAddress(kernel, "Heap32ListNext");
            process_first =
                (PROCESS32) GetProcAddress(kernel, "Process32First");
            process_next =
                (PROCESS32) GetProcAddress(kernel, "Process32Next");
            thread_first = (THREAD32) GetProcAddress(kernel, "Thread32First");
            thread_next = (THREAD32) GetProcAddress(kernel, "Thread32Next");
            module_first = (MODULE32) GetProcAddress(kernel, "Module32First");
            module_next = (MODULE32) GetProcAddress(kernel, "Module32Next");

            if (snap && heap_first && heap_next && heaplist_first &&
                heaplist_next && process_first && process_next &&
                thread_first && thread_next && module_first &&
                module_next && (handle = snap(TH32CS_SNAPALL, 0))
                != INVALID_HANDLE_VALUE) {
                /* heap list and heap walking */
                /*
                 * HEAPLIST32 contains 3 fields that will change with each
                 * entry.  Consider each field a source of 1 byte of entropy.
                 * HEAPENTRY32 contains 5 fields that will change with each
                 * entry.  Consider each field a source of 1 byte of entropy.
                 */
                ZeroMemory(&hlist, sizeof(HEAPLIST32));
                hlist.dwSize = sizeof(HEAPLIST32);
                if (good)
                    starttime = GetTickCount();
#  ifdef _MSC_VER
                if (heaplist_first(handle, &hlist)) {
                    /*
                     * following discussion on dev ML, exception on WinCE (or
                     * other Win platform) is theoretically of unknown
                     * origin; prevent infinite loop here when this
                     * theoretical case occurs; otherwise cope with the
                     * expected (MSDN documented) exception-throwing
                     * behaviour of Heap32Next() on WinCE.
                     *
                     * based on patch in original message by Tanguy Fautré
                     * (2009/03/02) Subject: RAND_poll() and
                     * CreateToolhelp32Snapshot() stability
                     */
                    int ex_cnt_limit = 42;
                    do {
                        RAND_add(&hlist, hlist.dwSize, 3);
                        __try {
                            ZeroMemory(&hentry, sizeof(HEAPENTRY32));
                            hentry.dwSize = sizeof(HEAPENTRY32);
                            if (heap_first(&hentry,
                                           hlist.th32ProcessID,
                                           hlist.th32HeapID)) {
                                int entrycnt = 80;
                                do
                                    RAND_add(&hentry, hentry.dwSize, 5);
                                while (heap_next(&hentry)
                                       && (!good || NOTTOOLONG(starttime))
                                       && --entrycnt > 0);
                            }
                        }
                        __except(EXCEPTION_EXECUTE_HANDLER) {
                            /*
                             * ignore access violations when walking the heap
                             * list
                             */
                            ex_cnt_limit--;
                        }
                    } while (heaplist_next(handle, &hlist)
                             && (!good || NOTTOOLONG(starttime))
                             && ex_cnt_limit > 0);
                }
#  else
                if (heaplist_first(handle, &hlist)) {
                    do {
                        RAND_add(&hlist, hlist.dwSize, 3);
                        hentry.dwSize = sizeof(HEAPENTRY32);
                        if (heap_first(&hentry,
                                       hlist.th32ProcessID,
                                       hlist.th32HeapID)) {
                            int entrycnt = 80;
                            do
                                RAND_add(&hentry, hentry.dwSize, 5);
                            while (heap_next(&hentry)
                                   && (!good || NOTTOOLONG(starttime))
                                   && --entrycnt > 0);
                        }
                    } while (heaplist_next(handle, &hlist)
                             && (!good || NOTTOOLONG(starttime)));
                }
#  endif

                /* process walking */
                /*
                 * PROCESSENTRY32 contains 9 fields that will change with
                 * each entry.  Consider each field a source of 1 byte of
                 * entropy.
                 */
                p.dwSize = sizeof(PROCESSENTRY32);

                if (good)
                    starttime = GetTickCount();
                if (process_first(handle, &p))
                    do
                        RAND_add(&p, p.dwSize, 9);
                    while (process_next(handle, &p)
                           && (!good || NOTTOOLONG(starttime)));

                /* thread walking */
                /*
                 * THREADENTRY32 contains 6 fields that will change with each
                 * entry.  Consider each field a source of 1 byte of entropy.
                 */
                t.dwSize = sizeof(THREADENTRY32);
                if (good)
                    starttime = GetTickCount();
                if (thread_first(handle, &t))
                    do
                        RAND_add(&t, t.dwSize, 6);
                    while (thread_next(handle, &t)
                           && (!good || NOTTOOLONG(starttime)));

                /* module walking */
                /*
                 * MODULEENTRY32 contains 9 fields that will change with each
                 * entry.  Consider each field a source of 1 byte of entropy.
                 */
                m.dwSize = sizeof(MODULEENTRY32);
                if (good)
                    starttime = GetTickCount();
                if (module_first(handle, &m))
                    do
                        RAND_add(&m, m.dwSize, 9);
                    while (module_next(handle, &m)
                           && (!good || NOTTOOLONG(starttime)));
                if (close_snap)
                    close_snap(handle);
                else
                    CloseHandle(handle);

            }

            FreeLibrary(kernel);
        }
    }
# endif                         /* !OPENSSL_SYS_WINCE */

    /* timer data */
    readtimer();

    /* memory usage statistics */
    GlobalMemoryStatus(&m);
    RAND_add(&m, sizeof(m), 1);

    /* process ID */
    w = GetCurrentProcessId();
    RAND_add(&w, sizeof(w), 1);

# if 0
    printf("Exiting RAND_poll\n");
# endif

    return (1);
}

int RAND_event(UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    double add_entropy = 0;

    switch (iMsg) {
    case WM_KEYDOWN:
        {
            static WPARAM key;
            if (key != wParam)
                add_entropy = 0.05;
            key = wParam;
        }
        break;
    case WM_MOUSEMOVE:
        {
            static int lastx, lasty, lastdx, lastdy;
            int x, y, dx, dy;

            x = LOWORD(lParam);
            y = HIWORD(lParam);
            dx = lastx - x;
            dy = lasty - y;
            if (dx != 0 && dy != 0 && dx - lastdx != 0 && dy - lastdy != 0)
                add_entropy = .2;
            lastx = x, lasty = y;
            lastdx = dx, lastdy = dy;
        }
        break;
    }

    readtimer();
    RAND_add(&iMsg, sizeof(iMsg), add_entropy);
    RAND_add(&wParam, sizeof(wParam), 0);
    RAND_add(&lParam, sizeof(lParam), 0);

    return (RAND_status());
}

void RAND_screen(void)
{                               /* function available for backward
                                 * compatibility */
    RAND_poll();
    readscreen();
}

/* feed timing information to the PRNG */
static void readtimer(void)
{
    DWORD w;
    LARGE_INTEGER l;
    static int have_perfc = 1;
# if defined(_MSC_VER) && defined(_M_X86)
    static int have_tsc = 1;
    DWORD cyclecount;

    if (have_tsc) {
        __try {
            __asm {
            _emit 0x0f _emit 0x31 mov cyclecount, eax}
            RAND_add(&cyclecount, sizeof(cyclecount), 1);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            have_tsc = 0;
        }
    }
# else
#  define have_tsc 0
# endif

    if (have_perfc) {
        if (QueryPerformanceCounter(&l) == 0)
            have_perfc = 0;
        else
            RAND_add(&l, sizeof(l), 0);
    }

    if (!have_tsc && !have_perfc) {
        w = GetTickCount();
        RAND_add(&w, sizeof(w), 0);
    }
}

/* feed screen contents to PRNG */
/*****************************************************************************
 *
 * Created 960901 by Gertjan van Oosten, gertjan@West.NL, West Consulting B.V.
 *
 * Code adapted from
 * <URL:http://support.microsoft.com/default.aspx?scid=kb;[LN];97193>;
 * the original copyright message is:
 *
 *   (C) Copyright Microsoft Corp. 1993.  All rights reserved.
 *
 *   You have a royalty-free right to use, modify, reproduce and
 *   distribute the Sample Files (and/or any modified version) in
 *   any way you find useful, provided that you agree that
 *   Microsoft has no warranty obligations or liability for any
 *   Sample Application Files which are modified.
 */

static void readscreen(void)
{
# if !defined(OPENSSL_SYS_WINCE) && !defined(OPENSSL_SYS_WIN32_CYGWIN)
    HDC hScrDC;                 /* screen DC */
    HBITMAP hBitmap;            /* handle for our bitmap */
    BITMAP bm;                  /* bitmap properties */
    unsigned int size;          /* size of bitmap */
    char *bmbits;               /* contents of bitmap */
    int w;                      /* screen width */
    int h;                      /* screen height */
    int y;                      /* y-coordinate of screen lines to grab */
    int n = 16;                 /* number of screen lines to grab at a time */
    BITMAPINFOHEADER bi;        /* info about the bitmap */

    if (check_winnt() && OPENSSL_isservice() > 0)
        return;

    /* Get a reference to the screen DC */
    hScrDC = GetDC(NULL);

    /* Get screen resolution */
    w = GetDeviceCaps(hScrDC, HORZRES);
    h = GetDeviceCaps(hScrDC, VERTRES);

    /* Create a bitmap compatible with the screen DC */
    hBitmap = CreateCompatibleBitmap(hScrDC, w, n);

    /* Get bitmap properties */
    GetObject(hBitmap, sizeof(bm), (LPSTR)&bm);
    size = (unsigned int)4 * bm.bmHeight * bm.bmWidth;
    bi.biSize = sizeof(bi);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    bmbits = OPENSSL_malloc(size);
    if (bmbits) {
        /* Now go through the whole screen, repeatedly grabbing n lines */
        for (y = 0; y < h - n; y += n) {
            unsigned char md[MD_DIGEST_LENGTH];

            /* Copy the bits of the current line range into the buffer */
            GetDIBits(hScrDC, hBitmap, y, n,
                      bmbits, (LPBITMAPINFO)&bi, DIB_RGB_COLORS);

            /* Get the hash of the bitmap */
            MD(bmbits, size, md);

            /* Seed the random generator with the hash value */
            RAND_add(md, MD_DIGEST_LENGTH, 0);
        }

        OPENSSL_free(bmbits);
    }

    /* Clean up */
    DeleteObject(hBitmap);
    ReleaseDC(NULL, hScrDC);
# endif                         /* !OPENSSL_SYS_WINCE */
}

#endif
