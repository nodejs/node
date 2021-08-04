// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_TURBO_ASSEMBLER_H_
#define V8_CODEGEN_TURBO_ASSEMBLER_H_

#include <memory>

#include "src/base/template-utils.h"
#include "src/builtins/builtins.h"
#include "src/codegen/assembler-arch.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

enum class JumpMode {
  kJump,          // Does a direct jump to the given address
  kPushAndReturn  // Pushes the given address as the current return address and
                  // does a return
};

// Common base class for platform-specific TurboAssemblers containing
// platform-independent bits.
// You will encounter two subclasses, TurboAssembler (derives from
// TurboAssemblerBase), and MacroAssembler (derives from TurboAssembler). The
// main difference is that MacroAssembler is allowed to access the isolate, and
// TurboAssembler accesses the isolate in a very limited way. TurboAssembler
// contains all the functionality that is used by Turbofan, and does not expect
// to be running on the main thread.
class V8_EXPORT_PRIVATE TurboAssemblerBase : public Assembler {
 public:
  // Constructors are declared public to inherit them in derived classes
  // with `using` directive.
  TurboAssemblerBase(Isolate* isolate, CodeObjectRequired create_code_object,
                     std::unique_ptr<AssemblerBuffer> buffer = {})
      : TurboAssemblerBase(isolate, AssemblerOptions::Default(isolate),
                           create_code_object, std::move(buffer)) {}

  TurboAssemblerBase(Isolate* isolate, const AssemblerOptions& options,
                     CodeObjectRequired create_code_object,
                     std::unique_ptr<AssemblerBuffer> buffer = {});

  Isolate* isolate() const {
    return isolate_;
  }

  Handle<HeapObject> CodeObject() const {
    DCHECK(!code_object_.is_null());
    return code_object_;
  }

  bool root_array_available() const { return root_array_available_; }
  void set_root_array_available(bool v) { root_array_available_ = v; }

  bool trap_on_abort() const { return trap_on_abort_; }

  bool should_abort_hard() const { return hard_abort_; }
  void set_abort_hard(bool v) { hard_abort_ = v; }

  void set_builtin_index(int i) { maybe_builtin_index_ = i; }

  void set_has_frame(bool v) { has_frame_ = v; }
  bool has_frame() const { return has_frame_; }

  virtual void Jump(const ExternalReference& reference) = 0;

  // Calls the builtin given by the Smi in |builtin|. If builtins are embedded,
  // the trampoline Code object on the heap is not used.
  virtual void CallBuiltinByIndex(Register builtin_index) = 0;

  // Calls/jumps to the given Code object. If builtins are embedded, the
  // trampoline Code object on the heap is not used.
  virtual void CallCodeObject(Register code_object) = 0;
  virtual void JumpCodeObject(Register code_object,
                              JumpMode jump_mode = JumpMode::kJump) = 0;

  // Loads the given Code object's entry point into the destination register.
  virtual void LoadCodeObjectEntry(Register destination,
                                   Register code_object) = 0;

  // Loads the given constant or external reference without embedding its direct
  // pointer. The produced code is isolate-independent.
  void IndirectLoadConstant(Register destination, Handle<HeapObject> object);
  void IndirectLoadExternalReference(Register destination,
                                     ExternalReference reference);

  virtual void LoadFromConstantsTable(Register destination,
                                      int constant_index) = 0;

  // Corresponds to: destination = kRootRegister + offset.
  virtual void LoadRootRegisterOffset(Register destination,
                                      intptr_t offset) = 0;

  // Corresponds to: destination = [kRootRegister + offset].
  virtual void LoadRootRelative(Register destination, int32_t offset) = 0;

  virtual void LoadRoot(Register destination, RootIndex index) = 0;

  virtual void Trap() = 0;
  virtual void DebugBreak() = 0;

  static int32_t RootRegisterOffsetForRootIndex(RootIndex root_index);
  static int32_t RootRegisterOffsetForBuiltinIndex(int builtin_index);

  // Returns the root-relative offset to reference.address().
  static intptr_t RootRegisterOffsetForExternalReference(
      Isolate* isolate, const ExternalReference& reference);

  // Returns the root-relative offset to the external reference table entry,
  // which itself contains reference.address().
  static int32_t RootRegisterOffsetForExternalReferenceTableEntry(
      Isolate* isolate, const ExternalReference& reference);

  // An address is addressable through kRootRegister if it is located within
  // isolate->root_register_addressable_region().
  static bool IsAddressableThroughRootRegister(
      Isolate* isolate, const ExternalReference& reference);

#if defined(V8_TARGET_OS_WIN) || defined(V8_TARGET_OS_MACOSX)
  // Minimum page size. We must touch memory once per page when expanding the
  // stack, to avoid access violations.
  static constexpr int kStackPageSize = 4 * KB;
#endif

  V8_INLINE void RecordCommentForOffHeapTrampoline(int builtin_index) {
    if (!FLAG_code_comments) return;
    std::ostringstream str;
    str << "[ Inlined Trampoline to " << Builtins::name(builtin_index);
    RecordComment(str.str().c_str());
  }

 protected:
  Isolate* const isolate_ = nullptr;

  // This handle will be patched with the code object on installation.
  Handle<HeapObject> code_object_;

  // Whether kRootRegister has been initialized.
  bool root_array_available_ = true;

  // Immediately trap instead of calling {Abort} when debug code fails.
  bool trap_on_abort_ = FLAG_trap_on_abort;

  // Emit a C call to abort instead of a runtime call.
  bool hard_abort_ = false;

  // May be set while generating builtins.
  int maybe_builtin_index_ = Builtins::kNoBuiltinId;

  bool has_frame_ = false;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TurboAssemblerBase);
};

// Avoids emitting calls to the {Builtins::kAbort} builtin when emitting debug
// code during the lifetime of this scope object.
class V8_NODISCARD HardAbortScope {
 public:
  explicit HardAbortScope(TurboAssemblerBase* assembler)
      : assembler_(assembler), old_value_(assembler->should_abort_hard()) {
    assembler_->set_abort_hard(true);
  }
  ~HardAbortScope() { assembler_->set_abort_hard(old_value_); }

 private:
  TurboAssemblerBase* assembler_;
  bool old_value_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_TURBO_ASSEMBLER_H_
