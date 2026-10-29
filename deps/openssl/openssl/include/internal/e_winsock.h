/*
 * Copyright 2022-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_E_WINSOCK_H
#define OSSL_E_WINSOCK_H
#pragma once

#ifdef WINDOWS
#if !defined(_WIN32_WINNT)
/*
 * The _WIN32_WINNT is described here:
 * https://learn.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=msvc-170
 * In a nutshell the macro defines minimal required Windows version where
 * the resulting application is guaranteed to run on. If left undefined here,
 * then the definition is provided by the Windows SDK found on host where
 * application is being built.
 *
 * OpenSSL defaults to version 0x501, which matches Windows XP, meaning the
 * compiled library will use APIs available on Windows XP and later.  User may
 * override the version specified here at build time using command as
 * follows:
 *     perl ./Configure "-D_WIN32_WINNT=0x...." ...
 *
 * The list of recognized constants (as found in the link above) is as follows:
 * 	0x0400 // Windows NT 4.0
 *	0x0500 // Windows 2000
 *	0x0501 // Windows XP
 *	0x0502 // Windows Server 2003
 *	0x0600 // Windows Vista, Windows Server 2008, Windows Vista
 *	0x0601 // Windows 7
 *	0x0602 // Windows 8
 *	0x0603 // Windows 8.1
 *	0x0A00 // Windows 10
 */
#define _WIN32_WINNT 0x0501
#endif
#if defined(_WIN32_WINNT)
/*
 * Just like defining _WIN32_WINNT including winsock2.h implies
 * certain "discipline" for maintaining [broad] binary compatibility.
 * As long as structures are invariant among Winsock versions,
 * it's sufficient to check for specific Winsock2 API availability
 * at run-time [DSO_global_lookup is recommended]...
 */
#include <winsock2.h>
#include <ws2tcpip.h>
/*
 * Clang-based C++Builder 10.3.3 toolchains cannot find C inline
 * definitions at link-time.  This header defines WspiapiLoad() as an
 * __inline function.  https://quality.embarcadero.com/browse/RSP-33806
 */
#if !defined(__BORLANDC__) || !defined(__clang__)
#include <wspiapi.h>
#endif
/* yes, they have to be #included prior to <windows.h> */
#endif
#include <windows.h>
#endif
#endif /* !(OSSL_E_WINSOCK_H) */
