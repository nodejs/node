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

// Author: kenton@google.com (Kenton Varda) and others
//
// Contains basic types and utilities used by the rest of the library.

#ifndef GOOGLE_PROTOBUF_COMMON_H__
#define GOOGLE_PROTOBUF_COMMON_H__

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <google/protobuf/stubs/port.h>
#include <google/protobuf/stubs/macros.h>
#include <google/protobuf/stubs/platform_macros.h>

#ifndef PROTOBUF_USE_EXCEPTIONS
#if defined(_MSC_VER) && defined(_CPPUNWIND)
  #define PROTOBUF_USE_EXCEPTIONS 1
#elif defined(__EXCEPTIONS)
  #define PROTOBUF_USE_EXCEPTIONS 1
#else
  #define PROTOBUF_USE_EXCEPTIONS 0
#endif
#endif

#if PROTOBUF_USE_EXCEPTIONS
#include <exception>
#endif
#if defined(__APPLE__)
#include <TargetConditionals.h>  // for TARGET_OS_IPHONE
#endif

#if defined(__ANDROID__) || defined(GOOGLE_PROTOBUF_OS_ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || defined(GOOGLE_PROTOBUF_OS_IPHONE)
#include <pthread.h>
#endif

#include <google/protobuf/port_def.inc>

namespace std {}

namespace google {
namespace protobuf {
namespace internal {

// Some of these constants are macros rather than const ints so that they can
// be used in #if directives.

// The current version, represented as a single integer to make comparison
// easier:  major * 10^6 + minor * 10^3 + micro
#define GOOGLE_PROTOBUF_VERSION 3009000

// A suffix string for alpha, beta or rc releases. Empty for stable releases.
#define GOOGLE_PROTOBUF_VERSION_SUFFIX ""

// The minimum header version which works with the current version of
// the library.  This constant should only be used by protoc's C++ code
// generator.
static const int kMinHeaderVersionForLibrary = 3009000;

// The minimum protoc version which works with the current version of the
// headers.
#define GOOGLE_PROTOBUF_MIN_PROTOC_VERSION 3009000

// The minimum header version which works with the current version of
// protoc.  This constant should only be used in VerifyVersion().
static const int kMinHeaderVersionForProtoc = 3009000;

// Verifies that the headers and libraries are compatible.  Use the macro
// below to call this.
void PROTOBUF_EXPORT VerifyVersion(int headerVersion, int minLibraryVersion,
                                   const char* filename);

// Converts a numeric version number to a string.
std::string PROTOBUF_EXPORT VersionString(int version);

}  // namespace internal

// Place this macro in your main() function (or somewhere before you attempt
// to use the protobuf library) to verify that the version you link against
// matches the headers you compiled against.  If a version mismatch is
// detected, the process will abort.
#define GOOGLE_PROTOBUF_VERIFY_VERSION                                    \
  ::google::protobuf::internal::VerifyVersion(                            \
    GOOGLE_PROTOBUF_VERSION, GOOGLE_PROTOBUF_MIN_LIBRARY_VERSION,         \
    __FILE__)


// ===================================================================
// from google3/util/utf8/public/unilib.h

class StringPiece;
namespace internal {

// Checks if the buffer contains structurally-valid UTF-8.  Implemented in
// structurally_valid.cc.
PROTOBUF_EXPORT bool IsStructurallyValidUTF8(const char* buf, int len);

inline bool IsStructurallyValidUTF8(const std::string& str) {
  return IsStructurallyValidUTF8(str.data(), static_cast<int>(str.length()));
}

// Returns initial number of bytes of structually valid UTF-8.
PROTOBUF_EXPORT int UTF8SpnStructurallyValid(const StringPiece& str);

// Coerce UTF-8 byte string in src_str to be
// a structurally-valid equal-length string by selectively
// overwriting illegal bytes with replace_char (typically ' ' or '?').
// replace_char must be legal printable 7-bit Ascii 0x20..0x7e.
// src_str is read-only.
//
// Returns pointer to output buffer, src_str.data() if no changes were made,
//  or idst if some bytes were changed. idst is allocated by the caller
//  and must be at least as big as src_str
//
// Optimized for: all structurally valid and no byte copying is done.
//
PROTOBUF_EXPORT char* UTF8CoerceToStructurallyValid(const StringPiece& str,
                                                    char* dst,
                                                    char replace_char);

}  // namespace internal


// ===================================================================
// Shutdown support.

// Shut down the entire protocol buffers library, deleting all static-duration
// objects allocated by the library or by generated .pb.cc files.
//
// There are two reasons you might want to call this:
// * You use a draconian definition of "memory leak" in which you expect
//   every single malloc() to have a corresponding free(), even for objects
//   which live until program exit.
// * You are writing a dynamically-loaded library which needs to clean up
//   after itself when the library is unloaded.
//
// It is safe to call this multiple times.  However, it is not safe to use
// any other part of the protocol buffers library after
// ShutdownProtobufLibrary() has been called. Furthermore this call is not
// thread safe, user needs to synchronize multiple calls.
PROTOBUF_EXPORT void ShutdownProtobufLibrary();

namespace internal {

// Register a function to be called when ShutdownProtocolBuffers() is called.
PROTOBUF_EXPORT void OnShutdown(void (*func)());
// Run an arbitrary function on an arg
PROTOBUF_EXPORT void OnShutdownRun(void (*f)(const void*), const void* arg);

template <typename T>
T* OnShutdownDelete(T* p) {
  OnShutdownRun([](const void* pp) { delete static_cast<const T*>(pp); }, p);
  return p;
}

}  // namespace internal

#if PROTOBUF_USE_EXCEPTIONS
class FatalException : public std::exception {
 public:
  FatalException(const char* filename, int line, const std::string& message)
      : filename_(filename), line_(line), message_(message) {}
  virtual ~FatalException() throw();

  virtual const char* what() const throw();

  const char* filename() const { return filename_; }
  int line() const { return line_; }
  const std::string& message() const { return message_; }

 private:
  const char* filename_;
  const int line_;
  const std::string message_;
};
#endif

// This is at the end of the file instead of the beginning to work around a bug
// in some versions of MSVC.
using std::string;

}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_COMMON_H__
