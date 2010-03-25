// Copyright 2010 the V8 project authors. All rights reserved.
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



#ifndef V8_MIPS_FRAMES_MIPS_H_
#define V8_MIPS_FRAMES_MIPS_H_


namespace v8 {
namespace internal {

// Register lists.
// Note that the bit values must match those used in actual instruction
// encoding.
static const int kNumRegs = 32;

static const RegList kJSCallerSaved =
  1 << 4 |  // a0
  1 << 5 |  // a1
  1 << 6 |  // a2
  1 << 7;   // a3

static const int kNumJSCallerSaved = 4;


// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns r0.code() == 0.
int JSCallerSavedCode(int n);


// Callee-saved registers preserved when switching from C to JavaScript.
static const RegList kCalleeSaved =
  // Saved temporaries.
  1 << 16 | 1 << 17 | 1 << 18 | 1 << 19 |
  1 << 20 | 1 << 21 | 1 << 22 | 1 << 23 |
  // gp, sp, fp
  1 << 28 | 1 << 29 | 1 << 30;

static const int kNumCalleeSaved = 11;


typedef Object* JSCallerSavedBuffer[kNumJSCallerSaved];


// ----------------------------------------------------

class StackHandlerConstants : public AllStatic {
 public:
  static const int kNextOffset  = 0 * kPointerSize;
  static const int kStateOffset = 1 * kPointerSize;
  static const int kFPOffset    = 2 * kPointerSize;
  static const int kPCOffset    = 3 * kPointerSize;

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
  // Must be the same as kDebugMarkOffset. Alias introduced when upgrading.
  static const int kCodeOffset = -1 * kPointerSize;

  static const int kSavedRegistersOffset = 0 * kPointerSize;

  // The caller fields are below the frame pointer on the stack.
  static const int kCallerFPOffset = +0 * kPointerSize;
  // The calling JS function is between FP and PC.
  static const int kCallerPCOffset = +1 * kPointerSize;

  // FP-relative displacement of the caller's SP.
  static const int kCallerSPDisplacement = +3 * kPointerSize;
};


class StandardFrameConstants : public AllStatic {
 public:
  static const int kExpressionsOffset = -3 * kPointerSize;
  static const int kMarkerOffset      = -2 * kPointerSize;
  static const int kContextOffset     = -1 * kPointerSize;
  static const int kCallerFPOffset    =  0 * kPointerSize;
  static const int kCallerPCOffset    = +1 * kPointerSize;
  static const int kCallerSPOffset    = +2 * kPointerSize;

  // Size of the MIPS 4 32-bit argument slots.
  // This is just an alias with a shorter name. Use it from now on.
  static const int kRArgsSlotsSize = 4 * kPointerSize;
  static const int kRegularArgsSlotsSize = kRArgsSlotsSize;

  // C/C++ argument slots size.
  static const int kCArgsSlotsSize = 4 * kPointerSize;
  // JS argument slots size.
  static const int kJSArgsSlotsSize = 0 * kPointerSize;
};


class JavaScriptFrameConstants : public AllStatic {
 public:
  // FP-relative.
  static const int kLocal0Offset = StandardFrameConstants::kExpressionsOffset;
  static const int kSavedRegistersOffset = +2 * kPointerSize;
  static const int kFunctionOffset = StandardFrameConstants::kMarkerOffset;

  // Caller SP-relative.
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

} }  // namespace v8::internal

#endif
