// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/eh-frame.h"

namespace v8 {
namespace internal {

static const int kR0DwarfCode = 0;
static const int kFpDwarfCode = 11;
static const int kSpDwarfCode = 13;
static const int kLrDwarfCode = 14;

const int EhFrameConstants::kCodeAlignmentFactor = 4;
const int EhFrameConstants::kDataAlignmentFactor = -4;

void EhFrameWriter::WriteReturnAddressRegisterCode() {
  WriteULeb128(kLrDwarfCode);
}

void EhFrameWriter::WriteInitialStateInCie() {
  SetBaseAddressRegisterAndOffset(fp, 0);
  RecordRegisterNotModified(lr);
}

// static
int EhFrameWriter::RegisterToDwarfCode(Register name) {
  switch (name.code()) {
    case kRegCode_fp:
      return kFpDwarfCode;
    case kRegCode_sp:
      return kSpDwarfCode;
    case kRegCode_lr:
      return kLrDwarfCode;
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
    case kSpDwarfCode:
      return "sp";
    case kLrDwarfCode:
      return "lr";
    default:
      UNIMPLEMENTED();
      return nullptr;
  }
}

#endif

}  // namespace internal
}  // namespace v8
