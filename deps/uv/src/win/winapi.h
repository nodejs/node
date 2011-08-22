/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef UV_WIN_WINAPI_H_
#define UV_WIN_WINAPI_H_

#include <windows.h>


/*
 * Ntdll headers
 */
#ifndef _NTDEF_
  typedef LONG NTSTATUS;
  typedef NTSTATUS *PNTSTATUS;
#endif

#ifndef STATUS_SUCCESS
  #define STATUS_SUCCESS ((NTSTATUS) 0x0)
#endif

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  } DUMMYUNIONNAME;
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _FILE_PIPE_LOCAL_INFORMATION {
  ULONG NamedPipeType;
  ULONG NamedPipeConfiguration;
  ULONG MaximumInstances;
  ULONG CurrentInstances;
  ULONG InboundQuota;
  ULONG ReadDataAvailable;
  ULONG OutboundQuota;
  ULONG WriteQuotaAvailable;
  ULONG NamedPipeState;
  ULONG NamedPipeEnd;
} FILE_PIPE_LOCAL_INFORMATION, *PFILE_PIPE_LOCAL_INFORMATION;

typedef enum _FILE_INFORMATION_CLASS {
  FileDirectoryInformation = 1,
  FileFullDirectoryInformation,
  FileBothDirectoryInformation,
  FileBasicInformation,
  FileStandardInformation,
  FileInternalInformation,
  FileEaInformation,
  FileAccessInformation,
  FileNameInformation,
  FileRenameInformation,
  FileLinkInformation,
  FileNamesInformation,
  FileDispositionInformation,
  FilePositionInformation,
  FileFullEaInformation,
  FileModeInformation,
  FileAlignmentInformation,
  FileAllInformation,
  FileAllocationInformation,
  FileEndOfFileInformation,
  FileAlternateNameInformation,
  FileStreamInformation,
  FilePipeInformation,
  FilePipeLocalInformation,
  FilePipeRemoteInformation,
  FileMailslotQueryInformation,
  FileMailslotSetInformation,
  FileCompressionInformation,
  FileObjectIdInformation,
  FileCompletionInformation,
  FileMoveClusterInformation,
  FileQuotaInformation,
  FileReparsePointInformation,
  FileNetworkOpenInformation,
  FileAttributeTagInformation,
  FileTrackingInformation,
  FileIdBothDirectoryInformation,
  FileIdFullDirectoryInformation,
  FileValidDataLengthInformation,
  FileShortNameInformation,
  FileIoCompletionNotificationInformation,
  FileIoStatusBlockRangeInformation,
  FileIoPriorityHintInformation,
  FileSfioReserveInformation,
  FileSfioVolumeInformation,
  FileHardLinkInformation,
  FileProcessIdsUsingFileInformation,
  FileNormalizedNameInformation,
  FileNetworkPhysicalNameInformation,
  FileIdGlobalTxDirectoryInformation,
  FileIsRemoteDeviceInformation,
  FileAttributeCacheInformation,
  FileNumaNodeInformation,
  FileStandardLinkInformation,
  FileRemoteProtocolInformation,
  FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef ULONG (NTAPI *sRtlNtStatusToDosError)
              (NTSTATUS Status);

typedef NTSTATUS (NTAPI *sNtQueryInformationFile)
                 (HANDLE FileHandle,
                  PIO_STATUS_BLOCK IoStatusBlock,
                  PVOID FileInformation,
                  ULONG Length,
                  FILE_INFORMATION_CLASS FileInformationClass);


/*
 * Kernel32 headers
 */
#define FILE_SKIP_COMPLETION_PORT_ON_SUCCESS    0x1
#define FILE_SKIP_SET_EVENT_ON_HANDLE           0x2

#ifdef __MINGW32__
  typedef struct _OVERLAPPED_ENTRY {
      ULONG_PTR lpCompletionKey;
      LPOVERLAPPED lpOverlapped;
      ULONG_PTR Internal;
      DWORD dwNumberOfBytesTransferred;
  } OVERLAPPED_ENTRY, *LPOVERLAPPED_ENTRY;
#endif

typedef BOOL (WINAPI *sGetQueuedCompletionStatusEx)
             (HANDLE CompletionPort,
              LPOVERLAPPED_ENTRY lpCompletionPortEntries,
              ULONG ulCount,
              PULONG ulNumEntriesRemoved,
              DWORD dwMilliseconds,
              BOOL fAlertable);

typedef BOOL (WINAPI* sSetFileCompletionNotificationModes)
             (HANDLE FileHandle,
              UCHAR Flags);


/* Ntapi function pointers */
extern sRtlNtStatusToDosError pRtlNtStatusToDosError;
extern sNtQueryInformationFile pNtQueryInformationFile;


/* Kernel32 function pointers */
extern sGetQueuedCompletionStatusEx pGetQueuedCompletionStatusEx;
extern sSetFileCompletionNotificationModes pSetFileCompletionNotificationModes;

#endif /* UV_WIN_WINAPI_H_ */