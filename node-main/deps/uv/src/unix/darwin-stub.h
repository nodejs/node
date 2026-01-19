/* Copyright libuv project contributors. All rights reserved.
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

#ifndef UV_DARWIN_STUB_H_
#define UV_DARWIN_STUB_H_

#include <stdint.h>

struct CFArrayCallBacks;
struct CFRunLoopSourceContext;
struct FSEventStreamContext;

typedef double CFAbsoluteTime;
typedef double CFTimeInterval;
typedef int FSEventStreamEventFlags;
typedef int OSStatus;
typedef long CFIndex;
typedef struct CFArrayCallBacks CFArrayCallBacks;
typedef struct CFRunLoopSourceContext CFRunLoopSourceContext;
typedef struct FSEventStreamContext FSEventStreamContext;
typedef uint32_t FSEventStreamCreateFlags;
typedef uint64_t FSEventStreamEventId;
typedef unsigned CFStringEncoding;
typedef void* CFAllocatorRef;
typedef void* CFArrayRef;
typedef void* CFBundleRef;
typedef void* CFDictionaryRef;
typedef void* CFRunLoopRef;
typedef void* CFRunLoopSourceRef;
typedef void* CFStringRef;
typedef void* CFTypeRef;
typedef void* FSEventStreamRef;

typedef void (*FSEventStreamCallback)(const FSEventStreamRef,
                                      void*,
                                      size_t,
                                      void*,
                                      const FSEventStreamEventFlags*,
                                      const FSEventStreamEventId*);

struct CFRunLoopSourceContext {
  CFIndex version;
  void* info;
  void* pad[7];
  void (*perform)(void*);
};

struct FSEventStreamContext {
  CFIndex version;
  void* info;
  void* pad[3];
};

static const CFStringEncoding kCFStringEncodingUTF8 = 0x8000100;
static const OSStatus noErr = 0;

static const FSEventStreamEventId kFSEventStreamEventIdSinceNow = -1;

static const int kFSEventStreamCreateFlagNoDefer = 2;
static const int kFSEventStreamCreateFlagFileEvents = 16;

static const int kFSEventStreamEventFlagEventIdsWrapped = 8;
static const int kFSEventStreamEventFlagHistoryDone = 16;
static const int kFSEventStreamEventFlagItemChangeOwner = 0x4000;
static const int kFSEventStreamEventFlagItemCreated = 0x100;
static const int kFSEventStreamEventFlagItemFinderInfoMod = 0x2000;
static const int kFSEventStreamEventFlagItemInodeMetaMod = 0x400;
static const int kFSEventStreamEventFlagItemIsDir = 0x20000;
static const int kFSEventStreamEventFlagItemModified = 0x1000;
static const int kFSEventStreamEventFlagItemRemoved = 0x200;
static const int kFSEventStreamEventFlagItemRenamed = 0x800;
static const int kFSEventStreamEventFlagItemXattrMod = 0x8000;
static const int kFSEventStreamEventFlagKernelDropped = 4;
static const int kFSEventStreamEventFlagMount = 64;
static const int kFSEventStreamEventFlagRootChanged = 32;
static const int kFSEventStreamEventFlagUnmount = 128;
static const int kFSEventStreamEventFlagUserDropped = 2;

#endif  /* UV_DARWIN_STUB_H_ */
