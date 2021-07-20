// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/diagnostics/eh-frame.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

static const int kRaxDwarfCode = 0;
static const int kRbpDwarfCode = 6;
static const int kRspDwarfCode = 7;
static const int kRipDwarfCode = 16;

const int EhFrameConstants::kCodeAlignmentFactor = 1;
const int EhFrameConstants::kDataAlignmentFactor = -8;

void EhFrameWriter::WriteReturnAddressRegisterCode() {
  WriteULeb128(kRipDwarfCode);
}

void EhFrameWriter::WriteInitialStateInCie() {
  SetBaseAddressRegisterAndOffset(rsp, kSystemPointerSize);
  // x64 rip (r16) has no Register instance associated.
  RecordRegisterSavedToStack(kRipDwarfCode, -kSystemPointerSize);
}

// static
int EhFrameWriter::RegisterToDwarfCode(Register name) {
  switch (name.code()) {
    case kRegCode_rbp:
      return kRbpDwarfCode;
    case kRegCode_rsp:
      return kRspDwarfCode;
    case kRegCode_rax:
      return kRaxDwarfCode;
    default:
      UNIMPLEMENTED();
  }
}

#ifdef ENABLE_DISASSEMBLER

// static
const char* EhFrameDisassembler::DwarfRegisterCodeToString(int code) {
  switch (code) {
    case kRbpDwarfCode:
      return "rbp";
    case kRspDwarfCode:
      return "rsp";
    case kRipDwarfCode:
      return "rip";
    default:
      UNIMPLEMENTED();
  }
}

#endif

}  // namespace internal
}  // namespace v8
