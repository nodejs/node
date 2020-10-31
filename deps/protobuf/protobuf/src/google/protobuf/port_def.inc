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

// This file defines common macros that are used in protobuf.
//
// To hide these definitions from the outside world (and to prevent collisions
// if more than one version of protobuf is #included in the same project) you
// must follow this pattern when #including port_def.inc in a header file:
//
// #include "other_header.h"
// #include "message.h"
// // etc.
//
// #include "port_def.inc"  // MUST be last header included
//
// // Definitions for this header.
//
// #include "port_undef.inc"
//
// This is a textual header with no include guard, because we want to
// detect/prohibit anytime it is #included twice without a corresponding
// #undef.

// These macros are private and should always be
// ::util::RetrieveErrorSpace(*this) headers. If any of these errors fire, you
// should either properly #include port_undef.h at the end of your header that
// #includes port.h, or don't #include port.h twice in a .cc file.
#ifdef PROTOBUF_NAMESPACE
#error PROTOBUF_NAMESPACE was previously defined
#endif
#ifdef PROTOBUF_NAMESPACE_ID
#error PROTOBUF_NAMESPACE_ID was previously defined
#endif
#ifdef PROTOBUF_ALWAYS_INLINE
#error PROTOBUF_ALWAYS_INLINE was previously defined
#endif
#ifdef PROTOBUF_COLD
#error PROTOBUF_COLD was previously defined
#endif
#ifdef PROTOBUF_NOINLINE
#error PROTOBUF_NOINLINE was previously defined
#endif
#ifdef PROTOBUF_SECTION_VARIABLE
#error PROTOBUF_SECTION_VARIABLE was previously defined
#endif
#ifdef PROTOBUF_DEPRECATED
#error PROTOBUF_DEPRECATED was previously defined
#endif
#ifdef PROTOBUF_DEPRECATED_MSG
#error PROTOBUF_DEPRECATED_MSG was previously defined
#endif
#ifdef PROTOBUF_FUNC_ALIGN
#error PROTOBUF_FUNC_ALIGN was previously defined
#endif
#ifdef PROTOBUF_RETURNS_NONNULL
#error PROTOBUF_RETURNS_NONNULL was previously defined
#endif
#ifdef PROTOBUF_ATTRIBUTE_REINITIALIZES
#error PROTOBUF_ATTRIBUTE_REINITIALIZES was previously defined
#endif
#ifdef PROTOBUF_RTTI
#error PROTOBUF_RTTI was previously defined
#endif
#ifdef PROTOBUF_VERSION
#error PROTOBUF_VERSION was previously defined
#endif
#ifdef PROTOBUF_VERSION_SUFFIX
#error PROTOBUF_VERSION_SUFFIX was previously defined
#endif
#ifdef PROTOBUF_MIN_HEADER_VERSION_FOR_PROTOC
#error PROTOBUF_MIN_HEADER_VERSION_FOR_PROTOC was previously defined
#endif
#ifdef PROTOBUF_MIN_PROTOC_VERSION
#error PROTOBUF_MIN_PROTOC_VERSION was previously defined
#endif
#ifdef PROTOBUF_PREDICT_TRUE
#error PROTOBUF_PREDICT_TRUE was previously defined
#endif
#ifdef PROTOBUF_PREDICT_FALSE
#error PROTOBUF_PREDICT_FALSE was previously defined
#endif
#ifdef PROTOBUF_FIELD_OFFSET
#error PROTOBUF_FIELD_OFFSET was previously defined
#endif
#ifdef PROTOBUF_LL_FORMAT
#error PROTOBUF_LL_FORMAT was previously defined
#endif
#ifdef PROTOBUF_GUARDED_BY
#error PROTOBUF_GUARDED_BY was previously defined
#endif
#ifdef PROTOBUF_LONGLONG
#error PROTOBUF_LONGLONG was previously defined
#endif
#ifdef PROTOBUF_ULONGLONG
#error PROTOBUF_ULONGLONG was previously defined
#endif
#ifdef PROTOBUF_FALLTHROUGH_INTENDED
#error PROTOBUF_FALLTHROUGH_INTENDED was previously defined
#endif
#ifdef PROTOBUF_EXPORT
#error PROTOBUF_EXPORT was previously defined
#endif
#ifdef PROTOC_EXPORT
#error PROTOC_EXPORT was previously defined
#endif
#ifdef PROTOBUF_MUST_USE_RESULT
#error PROTOBUF_MUST_USE_RESULT was previously defined
#endif
#ifdef PROTOBUF_UNUSED
#error PROTOBUF_UNUSED was previously defined
#endif


#define PROTOBUF_NAMESPACE "google::protobuf"
#define PROTOBUF_NAMESPACE_ID google::protobuf
#define PROTOBUF_NAMESPACE_OPEN \
  namespace google {            \
  namespace protobuf {
#define PROTOBUF_NAMESPACE_CLOSE \
  } /* namespace protobuf */     \
  } /* namespace google */
#define PROTOBUF_DEPRECATED
#define PROTOBUF_DEPRECATED_MSG(x)
#define PROTOBUF_SECTION_VARIABLE(x)
#define PROTOBUF_MUST_USE_RESULT

// ----------------------------------------------------------------------------
// Annotations:  Some parts of the code have been annotated in ways that might
//   be useful to some compilers or tools, but are not supported universally.
//   You can #define these annotations yourself if the default implementation
//   is not right for you.

#ifdef GOOGLE_ATTRIBUTE_ALWAYS_INLINE
#define PROTOBUF_ALWAYS_INLINE GOOGLE_ATTRIBUTE_ALWAYS_INLINE
#else
#if defined(__GNUC__) && (__GNUC__ > 3 ||(__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
// For functions we want to force inline.
// Introduced in gcc 3.1.
#define PROTOBUF_ALWAYS_INLINE __attribute__ ((always_inline))
#else
// Other compilers will have to figure it out for themselves.
#define PROTOBUF_ALWAYS_INLINE
#endif
#endif

#ifdef GOOGLE_ATTRIBUTE_NOINLINE
#define PROTOBUF_NOINLINE GOOGLE_ATTRIBUTE_NOINLINE
#else
#if defined(__GNUC__) && (__GNUC__ > 3 ||(__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
// For functions we want to force not inline.
// Introduced in gcc 3.1.
#define PROTOBUF_NOINLINE __attribute__ ((noinline))
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
// Seems to have been around since at least Visual Studio 2005
#define PROTOBUF_NOINLINE __declspec(noinline)
#else
// Other compilers will have to figure it out for themselves.
#define PROTOBUF_NOINLINE
#endif
#endif

#ifdef GOOGLE_ATTRIBUTE_FUNC_ALIGN
#define PROTOBUF_FUNC_ALIGN GOOGLE_ATTRIBUTE_FUNC_ALIGN
#else
#if defined(__clang__) || \
    defined(__GNUC__) && (__GNUC__ > 4 ||(__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
// Function alignment attribute introduced in gcc 4.3
#define PROTOBUF_FUNC_ALIGN(bytes) __attribute__ ((aligned(bytes)))
#else
#define PROTOBUF_FUNC_ALIGN(bytes)
#endif
#endif

#ifdef GOOGLE_PREDICT_TRUE
#define PROTOBUF_PREDICT_TRUE GOOGLE_PREDICT_TRUE
#else
#ifdef __GNUC__
// Provided at least since GCC 3.0.
#define PROTOBUF_PREDICT_TRUE(x) (__builtin_expect(!!(x), 1))
#else
#define PROTOBUF_PREDICT_TRUE(x) (x)
#endif
#endif

#ifdef GOOGLE_PREDICT_FALSE
#define PROTOBUF_PREDICT_FALSE GOOGLE_PREDICT_FALSE
#else
#ifdef __GNUC__
// Provided at least since GCC 3.0.
#define PROTOBUF_PREDICT_FALSE(x) (__builtin_expect(x, 0))
#else
#define PROTOBUF_PREDICT_FALSE(x) (x)
#endif
#endif

#ifdef GOOGLE_PROTOBUF_ATTRIBUTE_RETURNS_NONNULL
#define PROTOBUF_RETURNS_NONNULL GOOGLE_PROTOBUF_ATTRIBUTE_RETURNS_NONNULL
#else
#ifdef __GNUC__
#define PROTOBUF_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define PROTOBUF_RETURNS_NONNULL
#endif
#endif

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::reinitializes)
#define PROTOBUF_ATTRIBUTE_REINITIALIZES [[clang::reinitializes]]
#endif
#endif
#ifndef PROTOBUF_ATTRIBUTE_REINITIALIZES
#define PROTOBUF_ATTRIBUTE_REINITIALIZES
#endif

#define PROTOBUF_GUARDED_BY(x)
#define PROTOBUF_COLD

// Copied from ABSL.
#if defined(__clang__) && defined(__has_warning)
#if __has_feature(cxx_attributes) && __has_warning("-Wimplicit-fallthrough")
#define PROTOBUF_FALLTHROUGH_INTENDED [[clang::fallthrough]]
#endif
#elif defined(__GNUC__) && __GNUC__ >= 7
#define PROTOBUF_FALLTHROUGH_INTENDED [[gnu::fallthrough]]
#endif

#ifndef PROTOBUF_FALLTHROUGH_INTENDED
#define PROTOBUF_FALLTHROUGH_INTENDED
#endif

#if defined(__has_cpp_attribute)
#define HAS_ATTRIBUTE(attr) __has_cpp_attribute(attr)
#else
#define HAS_ATTRIBUTE(attr) 0
#endif

#if HAS_ATTRIBUTE(unused) || (defined(__GNUC__) && !defined(__clang__))
#define PROTOBUF_UNUSED __attribute__((__unused__))
#else
#define PROTOBUF_UNUSED
#endif

#undef HAS_ATTRIBUTE

#ifdef _MSC_VER
#define PROTOBUF_LONGLONG(x) x##I64
#define PROTOBUF_ULONGLONG(x) x##UI64
#define PROTOBUF_LL_FORMAT "I64"  // As in printf("%I64d", ...)
#else
// By long long, we actually mean int64.
#define PROTOBUF_LONGLONG(x) x##LL
#define PROTOBUF_ULONGLONG(x) x##ULL
// Used to format real long long integers.
#define PROTOBUF_LL_FORMAT \
  "ll"  // As in "%lld". Note that "q" is poor form also.
#endif


// Shared google3/opensource definitions. //////////////////////////////////////

#define PROTOBUF_VERSION 3009000
#define PROTOBUF_MIN_HEADER_VERSION_FOR_PROTOC 3009000
#define PROTOBUF_MIN_PROTOC_VERSION 3009000
#define PROTOBUF_VERSION_SUFFIX ""

// The minimum library version which works with the current version of the
// headers.
#define GOOGLE_PROTOBUF_MIN_LIBRARY_VERSION 3009000

#if defined(GOOGLE_PROTOBUF_NO_RTTI) && GOOGLE_PROTOBUF_NO_RTTI
#define PROTOBUF_RTTI 0
#else
#define PROTOBUF_RTTI 1
#endif

// Returns the offset of the given field within the given aggregate type.
// This is equivalent to the ANSI C offsetof() macro.  However, according
// to the C++ standard, offsetof() only works on POD types, and GCC
// enforces this requirement with a warning.  In practice, this rule is
// unnecessarily strict; there is probably no compiler or platform on
// which the offsets of the direct fields of a class are non-constant.
// Fields inherited from superclasses *can* have non-constant offsets,
// but that's not what this macro will be used for.
#if defined(__clang__)
// For Clang we use __builtin_offsetof() and suppress the warning,
// to avoid Control Flow Integrity and UBSan vptr sanitizers from
// crashing while trying to validate the invalid reinterpet_casts.
#define PROTOBUF_FIELD_OFFSET(TYPE, FIELD) \
  _Pragma("clang diagnostic push")                                   \
  _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"")         \
  __builtin_offsetof(TYPE, FIELD)                                    \
  _Pragma("clang diagnostic pop")
#else
// Note that we calculate relative to the pointer value 16 here since if we
// just use zero, GCC complains about dereferencing a NULL pointer.  We
// choose 16 rather than some other number just in case the compiler would
// be confused by an unaligned pointer.
#define PROTOBUF_FIELD_OFFSET(TYPE, FIELD)                                \
  static_cast< ::google::protobuf::uint32>(reinterpret_cast<const char*>(                   \
                             &reinterpret_cast<const TYPE*>(16)->FIELD) - \
                         reinterpret_cast<const char*>(16))
#endif


#if defined(_MSC_VER) && defined(PROTOBUF_USE_DLLS)
#ifdef LIBPROTOBUF_EXPORTS
#define PROTOBUF_EXPORT __declspec(dllexport)
#else
#define PROTOBUF_EXPORT __declspec(dllimport)
#endif
#ifdef LIBPROTOC_EXPORTS
#define PROTOC_EXPORT __declspec(dllexport)
#else
#define PROTOC_EXPORT __declspec(dllimport)
#endif
#else
#define PROTOBUF_EXPORT
#define PROTOC_EXPORT
#endif

// Windows declares several inconvenient macro names.  We #undef them and then
// restore them in port_undef.inc.
#ifdef _MSC_VER
#pragma push_macro("GetMessage")
#undef GetMessage
#pragma push_macro("IGNORE")
#undef IGNORE
#pragma push_macro("IN")
#undef IN
#endif  // _MSC_VER

#if defined(__clang__)
#pragma clang diagnostic push
// TODO(gerbens) ideally we cleanup the code. But a cursory try shows many
// violations. So let's ignore for now.
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif
