// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_FRAME_CONSTANTS_H_
#define V8_EXECUTION_FRAME_CONSTANTS_H_

#include "src/common/globals.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {

// Every pointer in a frame has a slot id. On 32-bit platforms, doubles consume
// two slots.
//
// Stack slot indices >= 0 access the callee stack with slot 0 corresponding to
// the callee's saved return address and 1 corresponding to the saved frame
// pointer. Some frames have additional information stored in the fixed header,
// for example JSFunctions store the function context and marker in the fixed
// header, with slot index 2 corresponding to the current function context and 3
// corresponding to the frame marker/JSFunction.
//
// If V8_REVERSE_JSARGS is set, then the parameters are reversed in the stack,
// i.e., the first parameter (the receiver) is just above the return address.
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter 0   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |                 |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |  parameter n-1  |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter n   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   | [Constant Pool] |   |                        |
//       |- - - - - - - - -|   |                        |
// 2+cp  |Context/Frm. Type|   v   if a constant pool   |
//       |-----------------+----    is used, cp = 1,    |
// 3+cp  |                 |   ^   otherwise, cp = 0    |
//       |- - - - - - - - -|   |                        |
// 4+cp  |                 |   |                      Callee
//       |- - - - - - - - -|   |                   frame slots
//  ...  |                 | Frame slots           (slot >= 0)
//       |- - - - - - - - -|   |                        |
//       |                 |   v                        |
//  -----+-----------------+----- <-- stack ptr -------------
//
class CommonFrameConstants : public AllStatic {
 public:
  static constexpr int kCallerFPOffset = 0 * kSystemPointerSize;
  static constexpr int kCallerPCOffset = kCallerFPOffset + 1 * kFPOnStackSize;
  static constexpr int kCallerSPOffset = kCallerPCOffset + 1 * kPCOnStackSize;

  // Fixed part of the frame consists of return address, caller fp,
  // constant pool (if FLAG_enable_embedded_constant_pool), context, and
  // function. StandardFrame::IterateExpressions assumes that kLastObjectOffset
  // is the last object pointer.
  static constexpr int kFixedFrameSizeAboveFp = kPCOnStackSize + kFPOnStackSize;
  static constexpr int kFixedSlotCountAboveFp =
      kFixedFrameSizeAboveFp / kSystemPointerSize;
  static constexpr int kCPSlotSize =
      FLAG_enable_embedded_constant_pool ? kSystemPointerSize : 0;
  static constexpr int kCPSlotCount = kCPSlotSize / kSystemPointerSize;
  static constexpr int kConstantPoolOffset =
      kCPSlotSize ? -1 * kSystemPointerSize : 0;
  static constexpr int kContextOrFrameTypeSize = kSystemPointerSize;
  static constexpr int kContextOrFrameTypeOffset =
      -(kCPSlotSize + kContextOrFrameTypeSize);
};

// StandardFrames are used for interpreted and optimized JavaScript
// frames. They always have a context below the saved fp/constant
// pool and below that the JSFunction of the executing function.
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter 0   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |                 |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |  parameter n-1  |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter n   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   | [Constant Pool] |   |                        |
//       |- - - - - - - - -|   |                        |
// 2+cp  |     Context     |   |   if a constant pool   |
//       |- - - - - - - - -|   |    is used, cp = 1,    |
// 3+cp  |    JSFunction   |   v   otherwise, cp = 0    |
//       +-----------------+----                        |
// 4+cp  |                 |   ^                      Callee
//       |- - - - - - - - -|   |                   frame slots
//  ...  |                 | Frame slots           (slot >= 0)
//       |- - - - - - - - -|   |                        |
//       |                 |   v                        |
//  -----+-----------------+----- <-- stack ptr -------------
//
class StandardFrameConstants : public CommonFrameConstants {
 public:
  static constexpr int kFixedFrameSizeFromFp =
      2 * kSystemPointerSize + kCPSlotSize;
  static constexpr int kFixedFrameSize =
      kFixedFrameSizeAboveFp + kFixedFrameSizeFromFp;
  static constexpr int kFixedSlotCountFromFp =
      kFixedFrameSizeFromFp / kSystemPointerSize;
  static constexpr int kFixedSlotCount = kFixedFrameSize / kSystemPointerSize;
  static constexpr int kContextOffset = kContextOrFrameTypeOffset;
  static constexpr int kFunctionOffset = -2 * kSystemPointerSize - kCPSlotSize;
  static constexpr int kExpressionsOffset =
      -3 * kSystemPointerSize - kCPSlotSize;
  static constexpr int kLastObjectOffset = kContextOffset;
};

// OptimizedBuiltinFrameConstants are used for TF-generated builtins. They
// always have a context below the saved fp/constant pool and below that the
// JSFunction of the executing function and below that an integer (not a Smi)
// containing the number of arguments passed to the builtin.
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter 0   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |                 |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |  parameter n-1  |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter n   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   | [Constant Pool] |   |                        |
//       |- - - - - - - - -|   |                        |
// 2+cp  |     Context     |   |   if a constant pool   |
//       |- - - - - - - - -|   |    is used, cp = 1,    |
// 3+cp  |    JSFunction   |   |   otherwise, cp = 0    |
//       |- - - - - - - - -|   |                        |
// 4+cp  |      argc       |   v                        |
//       +-----------------+----                        |
// 5+cp  |                 |   ^                      Callee
//       |- - - - - - - - -|   |                   frame slots
//  ...  |                 | Frame slots           (slot >= 0)
//       |- - - - - - - - -|   |                        |
//       |                 |   v                        |
//  -----+-----------------+----- <-- stack ptr -------------
//
class OptimizedBuiltinFrameConstants : public StandardFrameConstants {
 public:
  static constexpr int kArgCSize = kSystemPointerSize;
  static constexpr int kArgCOffset = -3 * kSystemPointerSize - kCPSlotSize;
  static constexpr int kFixedFrameSize = kFixedFrameSizeAboveFp - kArgCOffset;
  static constexpr int kFixedSlotCount = kFixedFrameSize / kSystemPointerSize;
};

// TypedFrames have a SMI type maker value below the saved FP/constant pool to
// distinguish them from StandardFrames, which have a context in that position
// instead.
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter 0   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |                 |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |  parameter n-1  |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter n   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   | [Constant Pool] |   |                        |
//       |- - - - - - - - -|   |                        |
// 2+cp  |Frame Type Marker|   v   if a constant pool   |
//       |-----------------+----    is used, cp = 1,    |
// 3+cp  |                 |   ^   otherwise, cp = 0    |
//       |- - - - - - - - -|   |                        |
// 4+cp  |                 |   |                      Callee
//       |- - - - - - - - -|   |                   frame slots
//  ...  |                 | Frame slots           (slot >= 0)
//       |- - - - - - - - -|   |                        |
//       |                 |   v                        |
//  -----+-----------------+----- <-- stack ptr -------------
//
class TypedFrameConstants : public CommonFrameConstants {
 public:
  static constexpr int kFrameTypeSize = kContextOrFrameTypeSize;
  static constexpr int kFrameTypeOffset = kContextOrFrameTypeOffset;
  static constexpr int kFixedFrameSizeFromFp = kCPSlotSize + kFrameTypeSize;
  static constexpr int kFixedSlotCountFromFp =
      kFixedFrameSizeFromFp / kSystemPointerSize;
  static constexpr int kFixedFrameSize =
      StandardFrameConstants::kFixedFrameSizeAboveFp + kFixedFrameSizeFromFp;
  static constexpr int kFixedSlotCount = kFixedFrameSize / kSystemPointerSize;
  static constexpr int kFirstPushedFrameValueOffset =
      -StandardFrameConstants::kCPSlotSize - kFrameTypeSize -
      kSystemPointerSize;
};

#define TYPED_FRAME_PUSHED_VALUE_OFFSET(x) \
  (TypedFrameConstants::kFirstPushedFrameValueOffset - (x)*kSystemPointerSize)
#define TYPED_FRAME_SIZE(count) \
  (TypedFrameConstants::kFixedFrameSize + (count)*kSystemPointerSize)
#define TYPED_FRAME_SIZE_FROM_FP(count) \
  (TypedFrameConstants::kFixedFrameSizeFromFp + (count)*kSystemPointerSize)
#define DEFINE_TYPED_FRAME_SIZES(count)                                        \
  static constexpr int kFixedFrameSize = TYPED_FRAME_SIZE(count);              \
  static constexpr int kFixedSlotCount = kFixedFrameSize / kSystemPointerSize; \
  static constexpr int kFixedFrameSizeFromFp =                                 \
      TYPED_FRAME_SIZE_FROM_FP(count);                                         \
  static constexpr int kFixedSlotCountFromFp =                                 \
      kFixedFrameSizeFromFp / kSystemPointerSize

class ArgumentsAdaptorFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kFunctionOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kLengthOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  static constexpr int kPaddingOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(2);
  DEFINE_TYPED_FRAME_SIZES(3);
};

class BuiltinFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kFunctionOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kLengthOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);
};

class ConstructFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kContextOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kLengthOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  static constexpr int kConstructorOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(2);
  static constexpr int kPaddingOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(3);
  static constexpr int kNewTargetOrImplicitReceiverOffset =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(4);
  DEFINE_TYPED_FRAME_SIZES(5);
};

class CWasmEntryFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative:
  static constexpr int kCEntryFPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  DEFINE_TYPED_FRAME_SIZES(1);
};

class WasmCompiledFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kWasmInstanceOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  DEFINE_TYPED_FRAME_SIZES(1);
};

class WasmExitFrameConstants : public WasmCompiledFrameConstants {
 public:
  // FP-relative.
  static const int kCallingPCOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);
};

class BuiltinContinuationFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kFunctionOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kFrameSPtoFPDeltaAtDeoptimize =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  static constexpr int kBuiltinContextOffset =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(2);
  static constexpr int kBuiltinIndexOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(3);

  // The argument count is in the first allocatable register, stored below the
  // fixed part of the frame and therefore is not part of the fixed frame size.
  static constexpr int kArgCOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(4);
  DEFINE_TYPED_FRAME_SIZES(4);

  // Returns the number of padding stack slots needed when we have
  // 'register_count' register slots.
  // This is needed on some architectures to ensure the stack pointer is
  // aligned.
  static int PaddingSlotCount(int register_count);
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kLastExitFrameField = kSPOffset;
  DEFINE_TYPED_FRAME_SIZES(1);

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static constexpr int kCallerSPDisplacement = kCallerSPOffset;
};

// Behaves like an exit frame but with target and new target args.
class BuiltinExitFrameConstants : public CommonFrameConstants {
 public:
  static constexpr int kNewTargetOffset =
      kCallerPCOffset + 1 * kSystemPointerSize;
  static constexpr int kTargetOffset =
      kNewTargetOffset + 1 * kSystemPointerSize;
  static constexpr int kArgcOffset = kTargetOffset + 1 * kSystemPointerSize;
  static constexpr int kPaddingOffset = kArgcOffset + 1 * kSystemPointerSize;
  static constexpr int kFirstArgumentOffset =
      kPaddingOffset + 1 * kSystemPointerSize;
  static constexpr int kNumExtraArgsWithReceiver = 5;
};

class InterpreterFrameConstants : public AllStatic {
 public:
  // Fixed frame includes bytecode array and bytecode offset.
  static constexpr int kFixedFrameSize =
      StandardFrameConstants::kFixedFrameSize + 2 * kSystemPointerSize;
  static constexpr int kFixedFrameSizeFromFp =
      StandardFrameConstants::kFixedFrameSizeFromFp + 2 * kSystemPointerSize;

  // FP-relative.
#ifdef V8_REVERSE_JSARGS
  static constexpr int kFirstParamFromFp =
      StandardFrameConstants::kCallerSPOffset;
#else
  static constexpr int kLastParamFromFp =
      StandardFrameConstants::kCallerSPOffset;
#endif
  static constexpr int kCallerPCOffsetFromFp =
      StandardFrameConstants::kCallerPCOffset;
  static constexpr int kBytecodeArrayFromFp =
      -StandardFrameConstants::kFixedFrameSizeFromFp - 1 * kSystemPointerSize;
  static constexpr int kBytecodeOffsetFromFp =
      -StandardFrameConstants::kFixedFrameSizeFromFp - 2 * kSystemPointerSize;
  static constexpr int kRegisterFileFromFp =
      -StandardFrameConstants::kFixedFrameSizeFromFp - 3 * kSystemPointerSize;

  static constexpr int kExpressionsOffset = kRegisterFileFromFp;

  // Number of fixed slots in addition to a {StandardFrame}.
  static constexpr int kExtraSlotCount =
      InterpreterFrameConstants::kFixedFrameSize / kSystemPointerSize -
      StandardFrameConstants::kFixedFrameSize / kSystemPointerSize;

  // Expression index for {StandardFrame::GetExpressionAddress}.
  static constexpr int kBytecodeArrayExpressionIndex = -2;
  static constexpr int kBytecodeOffsetExpressionIndex = -1;
  static constexpr int kRegisterFileExpressionIndex = 0;

  // Returns the number of stack slots needed for 'register_count' registers.
  // This is needed because some architectures must pad the stack frame with
  // additional stack slots to ensure the stack pointer is aligned.
  static int RegisterStackSlotCount(int register_count);
};

inline static int FPOffsetToFrameSlot(int frame_offset) {
  return StandardFrameConstants::kFixedSlotCountAboveFp - 1 -
         frame_offset / kSystemPointerSize;
}

inline static int FrameSlotToFPOffset(int slot) {
  return (StandardFrameConstants::kFixedSlotCountAboveFp - 1 - slot) *
         kSystemPointerSize;
}

}  // namespace internal
}  // namespace v8

#if V8_TARGET_ARCH_IA32
#include "src/execution/ia32/frame-constants-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/execution/x64/frame-constants-x64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/execution/arm64/frame-constants-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/execution/arm/frame-constants-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
#include "src/execution/ppc/frame-constants-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/execution/mips/frame-constants-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/execution/mips64/frame-constants-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_S390
#include "src/execution/s390/frame-constants-s390.h"  // NOLINT
#else
#error Unsupported target architecture.
#endif

#endif  // V8_EXECUTION_FRAME_CONSTANTS_H_
