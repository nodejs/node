// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_CODE_STUBS_ARM64_H_
#define V8_ARM64_CODE_STUBS_ARM64_H_

namespace v8 {
namespace internal {

// Helper to call C++ functions from generated code. The caller must prepare
// the exit frame before doing the call with GenerateCall.
class DirectCEntryStub: public PlatformCodeStub {
 public:
  explicit DirectCEntryStub(Isolate* isolate) : PlatformCodeStub(isolate) {}
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  Movability NeedsImmovableCode() override { return kImmovable; }

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(DirectCEntry, PlatformCodeStub);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_CODE_STUBS_ARM64_H_
