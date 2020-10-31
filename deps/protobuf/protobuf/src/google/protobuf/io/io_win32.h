// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: laszlocsomor@google.com (Laszlo Csomor)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

// This file contains the declarations for Windows implementations of
// commonly used POSIX functions such as open(2) and access(2), as well
// as macro definitions for flags of these functions.
//
// By including this file you'll redefine open/access/etc. to
// ::google::protobuf::io::win32::{open/access/etc.}.
// Make sure you don't include a header that attempts to redeclare or
// redefine these functions, that'll lead to confusing compilation
// errors. It's best to #include this file as the last one to ensure that.
//
// This file is only used on Windows, it's empty on other platforms.

#ifndef GOOGLE_PROTOBUF_IO_IO_WIN32_H__
#define GOOGLE_PROTOBUF_IO_IO_WIN32_H__

#if defined(_WIN32)

#include <string>
#include <google/protobuf/port.h>

#include <google/protobuf/port_def.inc>

// Compilers on Windows other than MSVC (e.g. Cygwin, MinGW32) define the
// following functions already, except for mkdir.
namespace google {
namespace protobuf {
namespace io {
namespace win32 {

PROTOBUF_EXPORT FILE* fopen(const char* path, const char* mode);
PROTOBUF_EXPORT int access(const char* path, int mode);
PROTOBUF_EXPORT int chdir(const char* path);
PROTOBUF_EXPORT int close(int fd);
PROTOBUF_EXPORT int dup(int fd);
PROTOBUF_EXPORT int dup2(int fd1, int fd2);
PROTOBUF_EXPORT int mkdir(const char* path, int _mode);
PROTOBUF_EXPORT int open(const char* path, int flags, int mode = 0);
PROTOBUF_EXPORT int read(int fd, void* buffer, size_t size);
PROTOBUF_EXPORT int setmode(int fd, int mode);
PROTOBUF_EXPORT int stat(const char* path, struct _stat* buffer);
PROTOBUF_EXPORT int write(int fd, const void* buffer, size_t size);
PROTOBUF_EXPORT std::wstring testonly_utf8_to_winpath(const char* path);

namespace strings {

// Convert from UTF-16 to Active-Code-Page-encoded or to UTF-8-encoded text.
PROTOBUF_EXPORT bool wcs_to_mbs(const wchar_t* s, std::string* out,
                                bool outUtf8);

// Convert from Active-Code-Page-encoded or UTF-8-encoded text to UTF-16.
PROTOBUF_EXPORT bool mbs_to_wcs(const char* s, std::wstring* out, bool inUtf8);

// Convert from UTF-8-encoded text to UTF-16.
PROTOBUF_EXPORT bool utf8_to_wcs(const char* input, std::wstring* out);

// Convert from UTF-16-encoded text to UTF-8.
PROTOBUF_EXPORT bool wcs_to_utf8(const wchar_t* input, std::string* out);

}  // namespace strings

}  // namespace win32
}  // namespace io
}  // namespace protobuf
}  // namespace google

#ifndef W_OK
#define W_OK 02  // not defined by MSVC for whatever reason
#endif

#ifndef F_OK
#define F_OK 00  // not defined by MSVC for whatever reason
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#include <google/protobuf/port_undef.inc>

#endif  // defined(_WIN32)

#endif  // GOOGLE_PROTOBUF_IO_IO_WIN32_H__
