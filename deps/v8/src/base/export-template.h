// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_EXPORT_TEMPLATE_H_
#define V8_BASE_EXPORT_TEMPLATE_H_

// Synopsis
//
// This header provides macros for using FOO_EXPORT macros with explicit
// template instantiation declarations and definitions.
// Generally, the FOO_EXPORT macros are used at declarations,
// and GCC requires them to be used at explicit instantiation declarations,
// but MSVC requires __declspec(dllexport) to be used at the explicit
// instantiation definitions instead.

// Usage
//
// In a header file, write:
//
//   extern template class EXPORT_TEMPLATE_DECLARE(FOO_EXPORT) foo<bar>;
//
// In a source file, write:
//
//   template class EXPORT_TEMPLATE_DEFINE(FOO_EXPORT) foo<bar>;

// Implementation notes
//
// The implementation of this header uses some subtle macro semantics to
// detect what the provided FOO_EXPORT value was defined as and then
// to dispatch to appropriate macro definitions.  Unfortunately,
// MSVC's C preprocessor is rather non-compliant and requires special
// care to make it work.
//
// Issue 1.
//
//   #define F(x)
//   F()
//
// MSVC emits warning C4003 ("not enough actual parameters for macro
// 'F'), even though it's a valid macro invocation.  This affects the
// macros below that take just an "export" parameter, because export
// may be empty.
//
// As a workaround, we can add a dummy parameter and arguments:
//
//   #define F(x,_)
//   F(,)
//
// Issue 2.
//
//   #define F(x) G##x
//   #define Gj() ok
//   F(j())
//
// The correct replacement for "F(j())" is "ok", but MSVC replaces it
// with "Gj()".  As a workaround, we can pass the result to an
// identity macro to force MSVC to look for replacements again.  (This
// is why EXPORT_TEMPLATE_STYLE_3 exists.)

#define EXPORT_TEMPLATE_DECLARE(export) \
  EXPORT_TEMPLATE_INVOKE(DECLARE, EXPORT_TEMPLATE_STYLE(export, ), export)
#define EXPORT_TEMPLATE_DEFINE(export) \
  EXPORT_TEMPLATE_INVOKE(DEFINE, EXPORT_TEMPLATE_STYLE(export, ), export)

// INVOKE is an internal helper macro to perform parameter replacements
// and token pasting to chain invoke another macro.  E.g.,
//     EXPORT_TEMPLATE_INVOKE(DECLARE, DEFAULT, FOO_EXPORT)
// will export to call
//     EXPORT_TEMPLATE_DECLARE_DEFAULT(FOO_EXPORT, )
// (but with FOO_EXPORT expanded too).
#define EXPORT_TEMPLATE_INVOKE(which, style, export) \
  EXPORT_TEMPLATE_INVOKE_2(which, style, export)
#define EXPORT_TEMPLATE_INVOKE_2(which, style, export) \
  EXPORT_TEMPLATE_##which##_##style(export, )

// Default style is to apply the FOO_EXPORT macro at declaration sites.
#define EXPORT_TEMPLATE_DECLARE_DEFAULT(export, _) export
#define EXPORT_TEMPLATE_DEFINE_DEFAULT(export, _)

// The "MSVC hack" style is used when FOO_EXPORT is defined
// as __declspec(dllexport), which MSVC requires to be used at
// definition sites instead.
#define EXPORT_TEMPLATE_DECLARE_MSVC_HACK(export, _)
#define EXPORT_TEMPLATE_DEFINE_MSVC_HACK(export, _) export

// EXPORT_TEMPLATE_STYLE is an internal helper macro that identifies which
// export style needs to be used for the provided FOO_EXPORT macro definition.
// "", "__attribute__(...)", and "__declspec(dllimport)" are mapped
// to "DEFAULT"; while "__declspec(dllexport)" is mapped to "MSVC_HACK".
//
// It's implemented with token pasting to transform the __attribute__ and
// __declspec annotations into macro invocations.  E.g., if FOO_EXPORT is
// defined as "__declspec(dllimport)", it undergoes the following sequence of
// macro substitutions:
//     EXPORT_TEMPLATE_STYLE(FOO_EXPORT, )
//     EXPORT_TEMPLATE_STYLE_2(__declspec(dllimport), )
//     EXPORT_TEMPLATE_STYLE_3(EXPORT_TEMPLATE_STYLE_MATCH__declspec(dllimport))
//     EXPORT_TEMPLATE_STYLE_MATCH__declspec(dllimport)
//     EXPORT_TEMPLATE_STYLE_MATCH_DECLSPEC_dllimport
//     DEFAULT
#define EXPORT_TEMPLATE_STYLE(export, _) EXPORT_TEMPLATE_STYLE_2(export, )
#define EXPORT_TEMPLATE_STYLE_2(export, _) \
  EXPORT_TEMPLATE_STYLE_3(                 \
      EXPORT_TEMPLATE_STYLE_MATCH_foj3FJo5StF0OvIzl7oMxA##export)
#define EXPORT_TEMPLATE_STYLE_3(style) style

// Internal helper macros for EXPORT_TEMPLATE_STYLE.
//
// XXX: C++ reserves all identifiers containing "__" for the implementation,
// but "__attribute__" and "__declspec" already contain "__" and the token-paste
// operator can only add characters; not remove them.  To minimize the risk of
// conflict with implementations, we include "foj3FJo5StF0OvIzl7oMxA" (a random
// 128-bit string, encoded in Base64) in the macro name.
#define EXPORT_TEMPLATE_STYLE_MATCH_foj3FJo5StF0OvIzl7oMxA DEFAULT
#define EXPORT_TEMPLATE_STYLE_MATCH_foj3FJo5StF0OvIzl7oMxA__attribute__(...) \
  DEFAULT
#define EXPORT_TEMPLATE_STYLE_MATCH_foj3FJo5StF0OvIzl7oMxA__declspec(arg) \
  EXPORT_TEMPLATE_STYLE_MATCH_DECLSPEC_##arg

// Internal helper macros for EXPORT_TEMPLATE_STYLE.
#define EXPORT_TEMPLATE_STYLE_MATCH_DECLSPEC_dllexport MSVC_HACK
#define EXPORT_TEMPLATE_STYLE_MATCH_DECLSPEC_dllimport DEFAULT

// Sanity checks.
//
// EXPORT_TEMPLATE_TEST uses the same macro invocation pattern as
// EXPORT_TEMPLATE_DECLARE and EXPORT_TEMPLATE_DEFINE do to check that they're
// working correctly.  When they're working correctly, the sequence of macro
// replacements should go something like:
//
//     EXPORT_TEMPLATE_TEST(DEFAULT, __declspec(dllimport));
//
//     static_assert(EXPORT_TEMPLATE_INVOKE(TEST_DEFAULT,
//         EXPORT_TEMPLATE_STYLE(__declspec(dllimport), ),
//         __declspec(dllimport)), "__declspec(dllimport)");
//
//     static_assert(EXPORT_TEMPLATE_INVOKE(TEST_DEFAULT,
//         DEFAULT, __declspec(dllimport)), "__declspec(dllimport)");
//
//     static_assert(EXPORT_TEMPLATE_TEST_DEFAULT_DEFAULT(
//         __declspec(dllimport)), "__declspec(dllimport)");
//
//     static_assert(true, "__declspec(dllimport)");
//
// When they're not working correctly, a syntax error should occur instead.
#define EXPORT_TEMPLATE_TEST(want, export)                                 \
  static_assert(EXPORT_TEMPLATE_INVOKE(                                    \
                    TEST_##want, EXPORT_TEMPLATE_STYLE(export, ), export), \
                #export)
#define EXPORT_TEMPLATE_TEST_DEFAULT_DEFAULT(...) true
#define EXPORT_TEMPLATE_TEST_MSVC_HACK_MSVC_HACK(...) true

EXPORT_TEMPLATE_TEST(DEFAULT, );
EXPORT_TEMPLATE_TEST(DEFAULT, __attribute__((visibility("default"))));
EXPORT_TEMPLATE_TEST(MSVC_HACK, __declspec(dllexport));
EXPORT_TEMPLATE_TEST(DEFAULT, __declspec(dllimport));

#undef EXPORT_TEMPLATE_TEST
#undef EXPORT_TEMPLATE_TEST_DEFAULT_DEFAULT
#undef EXPORT_TEMPLATE_TEST_MSVC_HACK_MSVC_HACK

#endif  // V8_BASE_EXPORT_TEMPLATE_H_
