// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASMJS_ASM_NAMES_H_
#define V8_ASMJS_ASM_NAMES_H_

#define STDLIB_MATH_VALUE_LIST(V) \
  V(E)                            \
  V(LN10)                         \
  V(LN2)                          \
  V(LOG2E)                        \
  V(LOG10E)                       \
  V(PI)                           \
  V(SQRT1_2)                      \
  V(SQRT2)

// V(stdlib.Math.<name>, Name, wasm-opcode, asm-js-type)
#define STDLIB_MATH_FUNCTION_MONOMORPHIC_LIST(V) \
  V(acos, Acos, kExprF64Acos, dq2d)              \
  V(asin, Asin, kExprF64Asin, dq2d)              \
  V(atan, Atan, kExprF64Atan, dq2d)              \
  V(cos, Cos, kExprF64Cos, dq2d)                 \
  V(sin, Sin, kExprF64Sin, dq2d)                 \
  V(tan, Tan, kExprF64Tan, dq2d)                 \
  V(exp, Exp, kExprF64Exp, dq2d)                 \
  V(log, Log, kExprF64Log, dq2d)                 \
  V(atan2, Atan2, kExprF64Atan2, dqdq2d)         \
  V(pow, Pow, kExprF64Pow, dqdq2d)               \
  V(imul, Imul, kExprI32Mul, ii2s)               \
  V(clz32, Clz32, kExprI32Clz, i2s)

// V(stdlib.Math.<name>, Name, unused, asm-js-type)
#define STDLIB_MATH_FUNCTION_CEIL_LIKE_LIST(V) \
  V(ceil, Ceil, x, ceil_like)                  \
  V(floor, Floor, x, ceil_like)                \
  V(sqrt, Sqrt, x, ceil_like)

// V(stdlib.Math.<name>, Name, unused, asm-js-type)
#define STDLIB_MATH_FUNCTION_LIST(V)       \
  V(min, Min, x, minmax)                   \
  V(max, Max, x, minmax)                   \
  V(abs, Abs, x, abs)                      \
  V(fround, Fround, x, fround)             \
  STDLIB_MATH_FUNCTION_MONOMORPHIC_LIST(V) \
  STDLIB_MATH_FUNCTION_CEIL_LIKE_LIST(V)

// V(stdlib.<name>, wasm-load-type, wasm-store-type, wasm-type)
#define STDLIB_ARRAY_TYPE_LIST(V)    \
  V(Int8Array, Mem8S, Mem8, I32)     \
  V(Uint8Array, Mem8U, Mem8, I32)    \
  V(Int16Array, Mem16S, Mem16, I32)  \
  V(Uint16Array, Mem16U, Mem16, I32) \
  V(Int32Array, Mem, Mem, I32)       \
  V(Uint32Array, Mem, Mem, I32)      \
  V(Float32Array, Mem, Mem, F32)     \
  V(Float64Array, Mem, Mem, F64)

#define STDLIB_OTHER_LIST(V) \
  V(Infinity)                \
  V(NaN)                     \
  V(Math)

// clang-format off (for return)
#define KEYWORD_NAME_LIST(V) \
  V(arguments)               \
  V(break)                   \
  V(case)                    \
  V(const)                   \
  V(continue)                \
  V(default)                 \
  V(do)                      \
  V(else)                    \
  V(eval)                    \
  V(for)                     \
  V(function)                \
  V(if)                      \
  V(new)                     \
  V(return )                 \
  V(switch)                  \
  V(var)                     \
  V(while)
// clang-format on

// V(token-string, token-name)
#define LONG_SYMBOL_NAME_LIST(V) \
  V("<=", LE)                    \
  V(">=", GE)                    \
  V("==", EQ)                    \
  V("!=", NE)                    \
  V("<<", SHL)                   \
  V(">>", SAR)                   \
  V(">>>", SHR)                  \
  V("'use asm'", UseAsm)

// clang-format off
#define SIMPLE_SINGLE_TOKEN_LIST(V)                                     \
  V('+') V('-') V('*') V('%') V('~') V('^') V('&') V('|') V('(') V(')') \
  V('[') V(']') V('{') V('}') V(':') V(';') V(',') V('?')
// clang-format on

// V(name, value, string-name)
#define SPECIAL_TOKEN_LIST(V)            \
  V(kUninitialized, 0, "{uninitalized}") \
  V(kEndOfInput, -1, "{end of input}")   \
  V(kParseError, -2, "{parse error}")    \
  V(kUnsigned, -3, "{unsigned value}")   \
  V(kDouble, -4, "{double value}")

#endif
