// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm64/assembler-arm64-inl.h"
#include "src/eh-frame.h"

namespace v8 {
namespace internal {

static const int kX0DwarfCode = 0;
static const int kJsSpDwarfCode = 28;
static const int kFpDwarfCode = 29;
static const int kLrDwarfCode = 30;
static const int kCSpDwarfCode = 31;

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
    case Register::kCode_x28:
      return kJsSpDwarfCode;
    case Register::kCode_x29:
      return kFpDwarfCode;
    case Register::kCode_x30:
      return kLrDwarfCode;
    case Register::kCode_x31:
      return kCSpDwarfCode;
    case Register::kCode_x0:
      return kX0DwarfCode;
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
    case kLrDwarfCode:
      return "lr";
    case kJsSpDwarfCode:
      return "jssp";
    case kCSpDwarfCode:
      return "csp";  // This could be zr as well
    default:
      UNIMPLEMENTED();
      return nullptr;
  }
}

#endif

}  // namespace internal
}  // namespace v8
