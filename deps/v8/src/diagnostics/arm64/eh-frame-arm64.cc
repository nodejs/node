// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/arm64/assembler-arm64-inl.h"
#include "src/diagnostics/eh-frame.h"

namespace v8 {
namespace internal {

static const int kX0DwarfCode = 0;
static const int kFpDwarfCode = 29;
static const int kLrDwarfCode = 30;
static const int kSpDwarfCode = 31;

const int EhFrameConstants::kCodeAlignmentFactor = 4;
const int EhFrameConstants::kDataAlignmentFactor = -8;

void EhFrameWriter::WriteReturnAddressRegisterCode() {
  WriteULeb128(kLrDwarfCode);
}

void EhFrameWriter::WriteInitialStateInCie() {
  SetBaseAddressRegisterAndOffset(x29, 0);
  RecordRegisterNotModified(x30);
}

// static
int EhFrameWriter::RegisterToDwarfCode(Register name) {
  switch (name.code()) {
    case kRegCode_x29:
      return kFpDwarfCode;
    case kRegCode_x30:
      return kLrDwarfCode;
    case kSPRegInternalCode:
      return kSpDwarfCode;
    case kRegCode_x0:
      return kX0DwarfCode;
    default:
      UNIMPLEMENTED();
  }
}

#ifdef ENABLE_DISASSEMBLER

// static
const char* EhFrameDisassembler::DwarfRegisterCodeToString(int code) {
  switch (code) {
    case kFpDwarfCode:
      return "fp";
    case kLrDwarfCode:
      return "lr";
    case kSpDwarfCode:
      return "sp";  // This could be zr as well
    default:
      UNIMPLEMENTED();
  }
}

#endif

}  // namespace internal
}  // namespace v8
