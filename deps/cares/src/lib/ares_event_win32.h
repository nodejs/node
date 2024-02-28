/* MIT License
 *
 * Copyright (c) 2024 Brad House
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __ARES_EVENT_WIN32_H
#define __ARES_EVENT_WIN32_H

#ifdef _WIN32
#  ifdef HAVE_WINSOCK2_H
#    include <winsock2.h>
#  endif
#  ifdef HAVE_WS2TCPIP_H
#    include <ws2tcpip.h>
#  endif
#  ifdef HAVE_MSWSOCK_H
#    include <mswsock.h>
#  endif
#  ifdef HAVE_WINDOWS_H
#    include <windows.h>
#  endif

/* From winternl.h */

/* If WDK is not installed and not using MinGW, provide the needed definitions
 */
typedef LONG NTSTATUS;

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID    Pointer;
  };

  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef VOID(NTAPI *PIO_APC_ROUTINE)(PVOID            ApcContext,
                                     PIO_STATUS_BLOCK IoStatusBlock,
                                     ULONG            Reserved);

/* From ntstatus.h */
#  define STATUS_SUCCESS ((NTSTATUS)0x00000000)
#  ifndef STATUS_PENDING
#    define STATUS_PENDING ((NTSTATUS)0x00000103L)
#  endif
#  define STATUS_CANCELLED ((NTSTATUS)0xC0000120L)
#  define STATUS_NOT_FOUND ((NTSTATUS)0xC0000225L)

/* Not sure what headers might have these */
#  define IOCTL_AFD_POLL 0x00012024

#  define AFD_POLL_RECEIVE           0x0001
#  define AFD_POLL_RECEIVE_EXPEDITED 0x0002
#  define AFD_POLL_SEND              0x0004
#  define AFD_POLL_DISCONNECT        0x0008
#  define AFD_POLL_ABORT             0x0010
#  define AFD_POLL_LOCAL_CLOSE       0x0020
#  define AFD_POLL_ACCEPT            0x0080
#  define AFD_POLL_CONNECT_FAIL      0x0100

typedef struct _AFD_POLL_HANDLE_INFO {
  HANDLE   Handle;
  ULONG    Events;
  NTSTATUS Status;
} AFD_POLL_HANDLE_INFO, *PAFD_POLL_HANDLE_INFO;

typedef struct _AFD_POLL_INFO {
  LARGE_INTEGER        Timeout;
  ULONG                NumberOfHandles;
  ULONG                Exclusive;
  AFD_POLL_HANDLE_INFO Handles[1];
} AFD_POLL_INFO, *PAFD_POLL_INFO;

/* Prototypes for dynamically loaded functions from ntdll.dll */
typedef NTSTATUS(NTAPI *NtCancelIoFileEx_t)(HANDLE           FileHandle,
                                            PIO_STATUS_BLOCK IoRequestToCancel,
                                            PIO_STATUS_BLOCK IoStatusBlock);
typedef NTSTATUS(NTAPI *NtDeviceIoControlFile_t)(
  HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
  PIO_STATUS_BLOCK IoStatusBlock, ULONG IoControlCode, PVOID InputBuffer,
  ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength);

/* On UWP/Windows Store, these definitions aren't there for some reason */
#  ifndef SIO_BSP_HANDLE_POLL
#    define SIO_BSP_HANDLE_POLL 0x4800001D
#  endif

#  ifndef SIO_BASE_HANDLE
#    define SIO_BASE_HANDLE 0x48000022
#  endif

#  ifndef HANDLE_FLAG_INHERIT
#    define HANDLE_FLAG_INHERIT 0x00000001
#  endif

#endif /* _WIN32 */

#endif /* __ARES_EVENT_WIN32_H */
