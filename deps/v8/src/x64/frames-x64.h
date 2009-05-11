// Copyright 2009 the V8 project authors. All rights reserved.
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

#ifndef V8_X64_FRAMES_X64_H_
#define V8_X64_FRAMES_X64_H_

namespace v8 { namespace internal {

// TODO(x64): This is a stub, mostly just a copy of the ia32 bit version.
// This will all need to change to be correct for x64.

static const int kNumRegs = 8;
static const RegList kJSCallerSaved = 0;
static const int kNumJSCallerSaved = 5;
typedef Object* JSCallerSavedBuffer[kNumJSCallerSaved];

class StackHandlerConstants : public AllStatic {
 public:
  static const int kNextOffset  = -1 * kPointerSize;
  static const int kPPOffset    = -1 * kPointerSize;
  static const int kFPOffset    = -1 * kPointerSize;

  static const int kCodeOffset  = -1 * kPointerSize;

  static const int kStateOffset = -1 * kPointerSize;
  static const int kPCOffset    = -1 * kPointerSize;

  static const int kAddressDisplacement = -1 * kPointerSize;
  static const int kSize = kPCOffset + kPointerSize;
};


class EntryFrameConstants : public AllStatic {
 public:
  static const int kCallerFPOffset      = -1 * kPointerSize;

  static const int kFunctionArgOffset   = -1 * kPointerSize;
  static const int kReceiverArgOffset   = -1 * kPointerSize;
  static const int kArgcOffset          = -1 * kPointerSize;
  static const int kArgvOffset          = -1 * kPointerSize;
};


class ExitFrameConstants : public AllStatic {
 public:
  static const int kDebugMarkOffset = -1 * kPointerSize;
  static const int kSPOffset        = -1 * kPointerSize;

  static const int kPPDisplacement = -1 * kPointerSize;

  static const int kCallerFPOffset = -1 * kPointerSize;
  static const int kCallerPCOffset = -1 * kPointerSize;
};


class StandardFrameConstants : public AllStatic {
 public:
  static const int kExpressionsOffset = -1 * kPointerSize;
  static const int kMarkerOffset      = -1 * kPointerSize;
  static const int kContextOffset     = -1 * kPointerSize;
  static const int kCallerFPOffset    = -1 * kPointerSize;
  static const int kCallerPCOffset    = -1 * kPointerSize;
  static const int kCallerSPOffset    = -1 * kPointerSize;
};


class JavaScriptFrameConstants : public AllStatic {
 public:
  static const int kLocal0Offset = StandardFrameConstants::kExpressionsOffset;
  static const int kSavedRegistersOffset = -1 * kPointerSize;
  static const int kFunctionOffset = StandardFrameConstants::kMarkerOffset;

  static const int kParam0Offset   = -1 * kPointerSize;
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

#endif  // V8_X64_FRAMES_X64_H_
