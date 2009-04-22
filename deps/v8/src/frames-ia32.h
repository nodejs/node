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

#ifndef V8_FRAMES_IA32_H_
#define V8_FRAMES_IA32_H_

namespace v8 { namespace internal {


// Register lists
// Note that the bit values must match those used in actual instruction encoding
static const int kNumRegs = 8;


// Caller-saved registers
static const RegList kJSCallerSaved =
  1 << 0 |  // eax
  1 << 1 |  // ecx
  1 << 2 |  // edx
  1 << 3 |  // ebx - used as a caller-saved register in JavaScript code
  1 << 7;   // edi - callee function

static const int kNumJSCallerSaved = 5;

typedef Object* JSCallerSavedBuffer[kNumJSCallerSaved];

// ----------------------------------------------------


class StackHandlerConstants : public AllStatic {
 public:
  static const int kNextOffset  = 0 * kPointerSize;
  static const int kPPOffset    = 1 * kPointerSize;
  static const int kFPOffset    = 2 * kPointerSize;

  // TODO(1233780): Get rid of the code slot in stack handlers.
  static const int kCodeOffset  = 3 * kPointerSize;

  static const int kStateOffset = 4 * kPointerSize;
  static const int kPCOffset    = 5 * kPointerSize;

  static const int kAddressDisplacement = -1 * kPointerSize;
  static const int kSize = kPCOffset + kPointerSize;
};


class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset      = -6 * kPointerSize;

  static const int kFunctionArgOffset   = +3 * kPointerSize;
  static const int kReceiverArgOffset   = +4 * kPointerSize;
  static const int kArgcOffset          = +5 * kPointerSize;
  static const int kArgvOffset          = +6 * kPointerSize;
};


class ExitFrameConstants : public AllStatic {
 public:
  static const int kDebugMarkOffset = -2 * kPointerSize;
  static const int kSPOffset        = -1 * kPointerSize;

  // Let the parameters pointer for exit frames point just below the
  // frame structure on the stack (frame pointer and return address).
  static const int kPPDisplacement = +2 * kPointerSize;

  static const int kCallerFPOffset =  0 * kPointerSize;
  static const int kCallerPCOffset = +1 * kPointerSize;
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

  // CallerSP-relative (aka PP-relative)
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




  // C Entry frames:

  //    lower    |    Stack    |
  //  addresses  |      ^      |
  //             |      |      |
  //             |             |
  //             +-------------+
  //             |  entry_pc   |
  //             +-------------+ <--+ entry_sp
  //                    .           |
  //                    .           |
  //                    .           |
  //             +-------------+    |
  //          -3 |  entry_sp --+----+
  //      e      +-------------+
  //      n   -2 | C function  |
  //      t      +-------------+
  //      r   -1 |  caller_pp  |
  //      y      +-------------+ <--- fp (frame pointer, ebp)
  //           0 |  caller_fp  |
  //      f      +-------------+
  //      r    1 |  caller_pc  |
  //      a      +-------------+ <--- caller_sp (stack pointer, esp)
  //      m    2 |             |
  //      e      |  arguments  |
  //             |             |
  //             +- - - - - - -+
  //             |  argument0  |
  //             +=============+
  //             |             |
  //             |   caller    |
  //   higher    | expressions |
  //  addresses  |             |


  // Proper JS frames:

  //    lower    |    Stack    |
  //  addresses  |      ^      |
  //             |      |      |
  //             |             |
  // ----------- +=============+ <--- sp (stack pointer, esp)
  //             |  function   |
  //             +-------------+
  //             |             |
  //             | expressions |
  //             |             |
  //             +-------------+
  //      a      |             |
  //      c      |   locals    |
  //      t      |             |
  //      i      +- - - - - - -+ <---
  //      v   -4 |   local0    |   ^
  //      a      +-------------+   |
  //      t   -3 |    code     |   |
  //      i      +-------------+   |
  //      o   -2 |   context   |   | kLocal0Offset
  //      n      +-------------+   |
  //          -1 |  caller_pp  |   v
  //      f      +-------------+ <--- fp (frame pointer, ebp)
  //      r    0 |  caller_fp  |
  //      a      +-------------+
  //      m    1 |  caller_pc  |
  //      e      +-------------+ <--- caller_sp (incl. parameters)
  //           2 |             |
  //             | parameters  |
  //             |             |
  //             +- - - - - - -+ <---
  //          -2 | parameter0  |   ^
  //             +-------------+   | kParam0Offset
  //          -1 |  receiver   |   v
  // ----------- +=============+ <--- pp (parameter pointer, edi)
  //           0 |  function   |
  //             +-------------+
  //             |             |
  //             |   caller    |
  //   higher    | expressions |
  //  addresses  |             |


  // JS entry frames: When calling from C to JS, we construct two extra
  // frames: An entry frame (C) and a trampoline frame (JS). The
  // following pictures shows the two frames:

  //    lower    |    Stack    |
  //  addresses  |      ^      |
  //             |      |      |
  //             |             |
  // ----------- +=============+ <--- sp (stack pointer, esp)
  //             |             |
  //             | parameters  |
  //      t      |             |
  //      r      +- - - - - - -+
  //      a      | parameter0  |
  //      m      +-------------+
  //      p      |  receiver   |
  //      o      +-------------+ <---
  //      l      |  function   |   ^
  //      i      +-------------+   |
  //      n   -3 |    code     |   | kLocal0Offset
  //      e      +-------------+
  //          -2 |    NULL     | context is always NULL
  //             +-------------+
  //      f   -1 |    NULL     | caller pp is always NULL for entry frames
  //      r      +-------------+ <--- fp (frame pointer, ebp)
  //      a    0 |  caller fp  |
  //      m      +-------------+
  //      e    1 |  caller pc  |
  //             +-------------+ <--- caller_sp (incl. parameters)
  //             |      0      |
  // ----------- +=============+ <--- pp (parameter pointer, edi)
  //             |      0      |
  //             +-------------+ <---
  //                    .          ^
  //                    .          |  try-handler (HandlerOffsets::kSize)
  //                    .          v
  //             +-------------+ <---
  //          -5 | next top pp |
  //             +-------------+
  //      e   -4 | next top fp |
  //      n      +-------------+ <---
  //      t   -3 |     ebx     |   ^
  //      r      +-------------+   |
  //      y   -2 |     esi     |   |  callee-saved registers
  //             +-------------+   |
  //          -1 |     edi     |   v
  //      f      +-------------+ <--- fp
  //      r    0 |  caller fp  |
  //      a      +-------------+      pp == NULL (parameter pointer)
  //      m    1 |  caller pc  |
  //      e      +-------------+ <--- caller sp
  //           2 | code  entry |   ^
  //             +-------------+   |
  //           3 |  function   |   |
  //             +-------------+   |  arguments passed from C code
  //           4 |  receiver   |   |
  //             +-------------+   |
  //           5 |    argc     |   |
  //             +-------------+   |
  //           6 |    argv     |   v
  //             +-------------+ <---
  //             |             |
  //   higher    |             |
  //  addresses  |             |


} }  // namespace v8::internal

#endif  // V8_FRAMES_IA32_H_
