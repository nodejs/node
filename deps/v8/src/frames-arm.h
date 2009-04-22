// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#ifndef V8_FRAMES_ARM_H_
#define V8_FRAMES_ARM_H_

namespace v8 { namespace internal {


// The ARM ABI does not specify the usage of register r9, which may be reserved
// as the static base or thread register on some platforms, in which case we
// leave it alone. Adjust the value of kR9Available accordingly:
static const int kR9Available = 1;  // 1 if available to us, 0 if reserved


// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding
static const int kNumRegs = 16;


// Caller-saved/arguments registers
static const RegList kJSCallerSaved =
  1 << 0 |  // r0 a1
  1 << 1 |  // r1 a2
  1 << 2 |  // r2 a3
  1 << 3;   // r3 a4

static const int kNumJSCallerSaved = 4;

typedef Object* JSCallerSavedBuffer[kNumJSCallerSaved];

// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns r0.code() == 0
int JSCallerSavedCode(int n);


// Callee-saved registers preserved when switching from C to JavaScript
static const RegList kCalleeSaved =
  1 <<  4 |  //  r4 v1
  1 <<  5 |  //  r5 v2
  1 <<  6 |  //  r6 v3
  1 <<  7 |  //  r7 v4
  1 <<  8 |  //  r8 v5 (cp in JavaScript code)
  kR9Available
    <<  9 |  //  r9 v6
  1 << 10 |  // r10 v7 (pp in JavaScript code)
  1 << 11;   // r11 v8 (fp in JavaScript code)

static const int kNumCalleeSaved = 7 + kR9Available;


// ----------------------------------------------------


class StackHandlerConstants : public AllStatic {
 public:
  // TODO(1233780): Get rid of the code slot in stack handlers.
  static const int kCodeOffset  = 0 * kPointerSize;
  static const int kNextOffset  = 1 * kPointerSize;
  static const int kStateOffset = 2 * kPointerSize;
  static const int kPPOffset    = 3 * kPointerSize;
  static const int kFPOffset    = 4 * kPointerSize;
  static const int kPCOffset    = 5 * kPointerSize;

  static const int kAddressDisplacement = -1 * kPointerSize;
  static const int kSize = kPCOffset + kPointerSize;
};


class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset      = -3 * kPointerSize;
};


class ExitFrameConstants : public AllStatic {
 public:
  // Exit frames have a debug marker on the stack.
  static const int kSPDisplacement = -1 * kPointerSize;

  // The debug marker is just above the frame pointer.
  static const int kDebugMarkOffset = -1 * kPointerSize;

  static const int kSavedRegistersOffset = 0 * kPointerSize;

  // Let the parameters pointer for exit frames point just below the
  // frame structure on the stack.
  static const int kPPDisplacement = 3 * kPointerSize;

  // The caller fields are below the frame pointer on the stack.
  static const int kCallerFPOffset = +0 * kPointerSize;
  static const int kCallerPPOffset = +1 * kPointerSize;
  static const int kCallerPCOffset = +2 * kPointerSize;
};


class StandardFrameConstants : public AllStatic {
 public:
  static const int kExpressionsOffset = -3 * kPointerSize;
  static const int kMarkerOffset      = -2 * kPointerSize;
  static const int kContextOffset     = -1 * kPointerSize;
  static const int kCallerFPOffset    =  0 * kPointerSize;
  static const int kCallerPCOffset    = +1 * kPointerSize;
  static const int kCallerSPOffset    = +2 * kPointerSize;
};


class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kLocal0Offset = StandardFrameConstants::kExpressionsOffset;
  static const int kSavedRegistersOffset = +2 * kPointerSize;
  static const int kFunctionOffset = StandardFrameConstants::kMarkerOffset;

  // PP-relative.
  static const int kParam0Offset   = -2 * kPointerSize;
  static const int kReceiverOffset = -1 * kPointerSize;
};


class ArgumentsAdaptorFrameConstants : public AllStatic {
 public:
  static const int kLengthOffset = StandardFrameConstants::kExpressionsOffset;
};


class InternalFrameConstants : public AllStatic {
 public:
  static const int kCodeOffset = StandardFrameConstants::kExpressionsOffset;
};


inline Object* JavaScriptFrame::function_slot_object() const {
  const int offset = JavaScriptFrameConstants::kFunctionOffset;
  return Memory::Object_at(fp() + offset);
}


// ----------------------------------------------------




  //    lower    |    Stack    |
  //  addresses  |      ^      |
  //             |      |      |
  //             |             |
  //             |  JS frame   |
  //             |             |
  //             |             |
  // ----------- +=============+ <--- sp (stack pointer)
  //             |  function   |
  //             +-------------+
  //             +-------------+
  //             |             |
  //             | expressions |
  //             |             |
  //             +-------------+
  //             |             |
  //      a      |   locals    |
  //      c      |             |
  //      t      +- - - - - - -+ <---
  //      i   -4 |   local0    |   ^
  //      v      +-------------+   |
  //      a   -3 |    code     |   |
  //      t      +-------------+   | kLocal0Offset
  //      i   -2 |   context   |   |
  //      o      +-------------+   |
  //      n   -1 | args_length |   v
  //             +-------------+ <--- fp (frame pointer)
  //           0 |  caller_pp  |
  //      f      +-------------+
  //      r    1 |  caller_fp  |
  //      a      +-------------+
  //      m    2 |  sp_on_exit |  (pp if return, caller_sp if no return)
  //      e      +-------------+
  //           3 |  caller_pc  |
  //             +-------------+ <--- caller_sp (incl. parameters)
  //             |             |
  //             | parameters  |
  //             |             |
  //             +- - - - - - -+ <---
  //          -2 | parameter0  |   ^
  //             +-------------+   | kParam0Offset
  //          -1 |  receiver   |   v
  // ----------- +=============+ <--- pp (parameter pointer, r10)
  //           0 |  function   |
  //             +-------------+
  //             |             |
  //             |caller-saved |  (must be valid JS values, traversed during GC)
  //             |    regs     |
  //             |             |
  //             +-------------+
  //             |             |
  //             |   caller    |
  //   higher    | expressions |
  //  addresses  |             |
  //             |             |
  //             |  JS frame   |



  // Handler frames (part of expressions of JS frames):

  //    lower    |    Stack    |
  //  addresses  |      ^      |
  //             |      |      |
  //             |             |
  //      h      | expressions |
  //      a      |             |
  //      n      +-------------+
  //      d   -1 |    code     |
  //      l      +-------------+ <--- handler sp
  //      e    0 |   next_sp   |  link to next handler (next handler's sp)
  //      r      +-------------+
  //           1 |    state    |
  //      f      +-------------+
  //      r    2 |     pp      |
  //      a      +-------------+
  //      m    3 |     fp      |
  //      e      +-------------+
  //           4 |     pc      |
  //             +-------------+
  //             |             |
  //   higher    | expressions |
  //  addresses  |             |



  // JS entry frames: When calling from C to JS, we construct two extra
  // frames: An entry frame (C) and a trampoline frame (JS). The
  // following pictures shows the two frames:

  //    lower    |    Stack    |
  //  addresses  |      ^      |
  //             |      |      |
  //             |             |
  //             |  JS frame   |
  //             |             |
  //             |             |
  // ----------- +=============+ <--- sp (stack pointer)
  //             |             |
  //             | parameters  |
  //      t      |             |
  //      r      +- - - - - - -+
  //      a      | parameter0  |
  //      m      +-------------+
  //      p      |  receiver   |
  //      o      +-------------+
  //      l      |  function   |
  //      i      +-------------+
  //      n   -3 |    code     |
  //      e      +-------------+
  //          -2 |    NULL     |  context is always NULL
  //             +-------------+
  //      f   -1 |      0      |  args_length is always zero
  //      r      +-------------+ <--- fp (frame pointer)
  //      a    0 |    NULL     |  caller pp is always NULL for entries
  //      m      +-------------+
  //      e    1 |  caller_fp  |
  //             +-------------+
  //           2 |  sp_on_exit |  (caller_sp)
  //             +-------------+
  //           3 |  caller_pc  |
  // ----------- +=============+ <--- caller_sp == pp
  //                    .          ^
  //                    .          |  try-handler, fake, not GC'ed
  //                    .          v
  //             +-------------+ <---
  //          -2 | next top pp |
  //             +-------------+
  //          -1 | next top fp |
  //             +-------------+ <--- fp
  //             |     r4      |  r4-r9 holding non-JS values must be preserved
  //             +-------------+
  //      J      |     r5      |  before being initialized not to confuse GC
  //      S      +-------------+
  //             |     r6      |
  //             +-------------+
  //      e      |     r7      |
  //      n      +-------------+
  //      t      |     r8      |
  //      r      +-------------+
  //      y    [ |     r9      | ]  only if r9 available
  //             +-------------+
  //             |     r10     |
  //      f      +-------------+
  //      r      |     r11     |
  //      a      +-------------+
  //      m      |  caller_sp  |
  //      e      +-------------+
  //             |  caller_pc  |
  //             +-------------+ <--- caller_sp
  //             |    argv     |    passed on stack from C code
  //             +-------------+
  //             |             |
  //   higher    |             |
  //  addresses  |   C frame   |


  // The first 4 args are passed from C in r0-r3 and are not spilled on entry:
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // [sp+0]: argv


  // C entry frames: When calling from JS to C, we construct one extra
  // frame:

  //    lower    |    Stack    |
  //  addresses  |      ^      |
  //             |      |      |
  //             |             |
  //             |   C frame   |
  //             |             |
  //             |             |
  // ----------- +=============+ <--- sp (stack pointer)
  //             |             |
  //             | parameters  |  (first 4 args are passed in r0-r3)
  //             |             |
  //             +-------------+ <--- fp (frame pointer)
  //      f  4/5 |  caller_fp  |
  //      r      +-------------+
  //      a  5/6 |  sp_on_exit |  (pp)
  //      m      +-------------+
  //      e  6/7 |  caller_pc  |
  //             +-------------+ <--- caller_sp (incl. parameters)
  //         7/8 |             |
  //             | parameters  |
  //             |             |
  //             +- - - - - - -+ <---
  //          -2 | parameter0  |   ^
  //             +-------------+   | kParam0Offset
  //          -1 |  receiver   |   v
  // ----------- +=============+ <--- pp (parameter pointer, r10)
  //           0 |  function   |
  //             +-------------+
  //             |             |
  //             |caller-saved |
  //             |    regs     |
  //             |             |
  //             +-------------+
  //             |             |
  //             |   caller    |
  //             | expressions |
  //             |             |
  //   higher    |             |
  //  addresses  |  JS frame   |


} }  // namespace v8::internal

#endif  // V8_FRAMES_ARM_H_
