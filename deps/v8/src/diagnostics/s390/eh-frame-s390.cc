// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/s390/assembler-s390-inl.h"
#include "src/diagnostics/eh-frame.h"

namespace v8 {
namespace internal {

static const int kR0DwarfCode = 0;
static const int kFpDwarfCode = 11;   // frame-pointer
static const int kR14DwarfCode = 14;  // return-address(lr)
static const int kSpDwarfCode = 15;   // stack-pointer

const int EhFrameConstants::kCodeAlignmentFactor = 2;  // 1 or 2 in s390
const int EhFrameConstants::kDataAlignmentFactor = -8;

void EhFrameWriter::WriteReturnAddressRegisterCode() {
  WriteULeb128(kR14DwarfCode);
}

void EhFrameWriter::WriteInitialStateInCie() {
  SetBaseAddressRegisterAndOffset(fp, 0);
  RecordRegisterNotModified(r14);
}

// static
int EhFrameWriter::RegisterToDwarfCode(Register name) {
  switch (name.code()) {
    case kRegCode_fp:
      return kFpDwarfCode;
    case kRegCode_r14:
      return kR14DwarfCode;
    case kRegCode_sp:
      return kSpDwarfCode;
    case kRegCode_r0:
      return kR0DwarfCode;
    default:
      UNIMPLEMENTED();
      return -1;
  }
}

#ifdef ENABLE_DISASSEMBLER

// static
const char* EhFrameDisassembler::DwarfRegisterCodeToString(int code) {
  switch (code) {
    case kFpDwarfCode:
      return "fp";
    case kR14DwarfCode:
      return "lr";
    case kSpDwarfCode:
      return "sp";
    default:
      UNIMPLEMENTED();
      return nullptr;
  }
}

#endif

}  // namespace internal
}  // namespace v8
