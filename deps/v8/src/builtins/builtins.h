// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_H_
#define V8_BUILTINS_BUILTINS_H_

#include "src/base/flags.h"
#include "src/builtins/builtins-definitions.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class ByteArray;
class CallInterfaceDescriptor;
class Callable;
template <typename T>
class Handle;
class Isolate;

// Forward declarations.
class BytecodeOffset;
class RootVisitor;
enum class InterpreterPushArgsMode : unsigned;
namespace compiler {
class CodeAssemblerState;
}  // namespace compiler

template <typename T>
static constexpr T FirstFromVarArgs(T x, ...) noexcept {
  return x;
}

// Convenience macro to avoid generating named accessors for all builtins.
#define BUILTIN_CODE(isolate, name) \
  (isolate)->builtins()->code_handle(i::Builtin::k##name)

enum class Builtin : int32_t {
  kNoBuiltinId = -1,
#define DEF_ENUM(Name, ...) k##Name,
  BUILTIN_LIST(DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM,
               DEF_ENUM)
#undef DEF_ENUM
#define EXTRACT_NAME(Name, ...) k##Name,
  // Define kFirstBytecodeHandler,
  kFirstBytecodeHandler =
      FirstFromVarArgs(BUILTIN_LIST_BYTECODE_HANDLERS(EXTRACT_NAME) 0)
#undef EXTRACT_NAME
};

V8_INLINE constexpr bool operator<(Builtin a, Builtin b) {
  using type = typename std::underlying_type<Builtin>::type;
  return static_cast<type>(a) < static_cast<type>(b);
}

V8_INLINE Builtin operator++(Builtin& builtin) {
  using type = typename std::underlying_type<Builtin>::type;
  return builtin = static_cast<Builtin>(static_cast<type>(builtin) + 1);
}

class Builtins {
 public:
  explicit Builtins(Isolate* isolate) : isolate_(isolate) {}

  Builtins(const Builtins&) = delete;
  Builtins& operator=(const Builtins&) = delete;

  void TearDown();

  // Disassembler support.
  const char* Lookup(Address pc);

#define ADD_ONE(Name, ...) +1
  static constexpr int kBuiltinCount = 0 BUILTIN_LIST(
      ADD_ONE, ADD_ONE, ADD_ONE, ADD_ONE, ADD_ONE, ADD_ONE, ADD_ONE);
  static constexpr int kBuiltinTier0Count = 0 BUILTIN_LIST_TIER0(
      ADD_ONE, ADD_ONE, ADD_ONE, ADD_ONE, ADD_ONE, ADD_ONE, ADD_ONE);
#undef ADD_ONE

  static constexpr Builtin kFirst = static_cast<Builtin>(0);
  static constexpr Builtin kLast = static_cast<Builtin>(kBuiltinCount - 1);
  static constexpr Builtin kLastTier0 =
      static_cast<Builtin>(kBuiltinTier0Count - 1);

  static constexpr int kFirstWideBytecodeHandler =
      static_cast<int>(Builtin::kFirstBytecodeHandler) +
      kNumberOfBytecodeHandlers;
  static constexpr int kFirstExtraWideBytecodeHandler =
      kFirstWideBytecodeHandler + kNumberOfWideBytecodeHandlers;
  static constexpr int kLastBytecodeHandlerPlusOne =
      kFirstExtraWideBytecodeHandler + kNumberOfWideBytecodeHandlers;
  STATIC_ASSERT(kLastBytecodeHandlerPlusOne == kBuiltinCount);

  static constexpr bool IsBuiltinId(Builtin builtin) {
    return builtin != Builtin::kNoBuiltinId;
  }
  static constexpr bool IsBuiltinId(int maybe_id) {
    STATIC_ASSERT(static_cast<int>(Builtin::kNoBuiltinId) == -1);
    return static_cast<uint32_t>(maybe_id) <
           static_cast<uint32_t>(kBuiltinCount);
  }
  static constexpr bool IsTier0(Builtin builtin) {
    return builtin <= kLastTier0 && IsBuiltinId(builtin);
  }

  static constexpr Builtin FromInt(int id) {
    DCHECK(IsBuiltinId(id));
    return static_cast<Builtin>(id);
  }
  static constexpr int ToInt(Builtin id) {
    DCHECK(IsBuiltinId(id));
    return static_cast<int>(id);
  }

  // The different builtin kinds are documented in builtins-definitions.h.
  enum Kind { CPP, TFJ, TFC, TFS, TFH, BCH, ASM };

  static BytecodeOffset GetContinuationBytecodeOffset(Builtin builtin);
  static Builtin GetBuiltinFromBytecodeOffset(BytecodeOffset);

  static constexpr Builtin GetRecordWriteStub(
      RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode) {
    switch (remembered_set_action) {
      case RememberedSetAction::kEmit:
        switch (fp_mode) {
          case SaveFPRegsMode::kIgnore:
            return Builtin::kRecordWriteEmitRememberedSetIgnoreFP;
          case SaveFPRegsMode::kSave:
            return Builtin::kRecordWriteEmitRememberedSetSaveFP;
        }
      case RememberedSetAction::kOmit:
        switch (fp_mode) {
          case SaveFPRegsMode::kIgnore:
            return Builtin::kRecordWriteOmitRememberedSetIgnoreFP;
          case SaveFPRegsMode::kSave:
            return Builtin::kRecordWriteOmitRememberedSetSaveFP;
        }
    }
  }

  static constexpr Builtin GetEphemeronKeyBarrierStub(SaveFPRegsMode fp_mode) {
    switch (fp_mode) {
      case SaveFPRegsMode::kIgnore:
        return Builtin::kEphemeronKeyBarrierIgnoreFP;
      case SaveFPRegsMode::kSave:
        return Builtin::kEphemeronKeyBarrierSaveFP;
    }
  }

  // Convenience wrappers.
  Handle<CodeT> CallFunction(ConvertReceiverMode = ConvertReceiverMode::kAny);
  Handle<CodeT> Call(ConvertReceiverMode = ConvertReceiverMode::kAny);
  Handle<CodeT> NonPrimitiveToPrimitive(
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);
  Handle<CodeT> OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint);

  // Used by CreateOffHeapTrampolines in isolate.cc.
  void set_code(Builtin builtin, CodeT code);

  V8_EXPORT_PRIVATE CodeT code(Builtin builtin);
  V8_EXPORT_PRIVATE Handle<CodeT> code_handle(Builtin builtin);

  static CallInterfaceDescriptor CallInterfaceDescriptorFor(Builtin builtin);
  V8_EXPORT_PRIVATE static Callable CallableFor(Isolate* isolate,
                                                Builtin builtin);
  static bool HasJSLinkage(Builtin builtin);

  static int GetStackParameterCount(Builtin builtin);

  static const char* name(Builtin builtin);

  // Support for --print-builtin-size and --print-builtin-code.
  void PrintBuiltinCode();
  void PrintBuiltinSize();

  // Returns the C++ entry point for builtins implemented in C++, and the null
  // Address otherwise.
  static Address CppEntryOf(Builtin builtin);

  static Kind KindOf(Builtin builtin);
  static const char* KindNameOf(Builtin builtin);

  static bool IsCpp(Builtin builtin);

  // True, iff the given code object is a builtin. Note that this does not
  // necessarily mean that its kind is Code::BUILTIN.
  static bool IsBuiltin(const Code code);

  // As above, but safe to access off the main thread since the check is done
  // by handle location. Similar to Heap::IsRootHandle.
  bool IsBuiltinHandle(Handle<HeapObject> maybe_code, Builtin* index) const;

  // True, iff the given code object is a builtin with off-heap embedded code.
  static bool IsIsolateIndependentBuiltin(const Code code);

  // True, iff the given builtin contains no isolate-specific code and can be
  // embedded into the binary.
  static constexpr bool kAllBuiltinsAreIsolateIndependent = true;
  static constexpr bool AllBuiltinsAreIsolateIndependent() {
    return kAllBuiltinsAreIsolateIndependent;
  }
  static constexpr bool IsIsolateIndependent(Builtin builtin) {
    STATIC_ASSERT(kAllBuiltinsAreIsolateIndependent);
    return kAllBuiltinsAreIsolateIndependent;
  }

  static void InitializeIsolateDataTables(Isolate* isolate);

  // Emits a CodeCreateEvent for every builtin.
  static void EmitCodeCreateEvents(Isolate* isolate);

  bool is_initialized() const { return initialized_; }

  // Used by SetupIsolateDelegate and Deserializer.
  void MarkInitialized() {
    DCHECK(!initialized_);
    initialized_ = true;
  }

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> InvokeApiFunction(
      Isolate* isolate, bool is_construct, Handle<HeapObject> function,
      Handle<Object> receiver, int argc, Handle<Object> args[],
      Handle<HeapObject> new_target);

  static void Generate_Adaptor(MacroAssembler* masm, Address builtin_address);

  static void Generate_CEntry(MacroAssembler* masm, int result_size,
                              SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                              bool builtin_exit_frame);

  static bool AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                   Handle<JSObject> target_global_proxy);

  // Creates a trampoline code object that jumps to the given off-heap entry.
  // The result should not be used directly, but only from the related Factory
  // function.
  // TODO(delphick): Come up with a better name since it may not generate an
  // executable trampoline.
  static Handle<Code> GenerateOffHeapTrampolineFor(
      Isolate* isolate, Address off_heap_entry, int32_t kind_specific_flags,
      bool generate_jump_to_instruction_stream);

  // Generate the RelocInfo ByteArray that would be generated for an offheap
  // trampoline.
  static Handle<ByteArray> GenerateOffHeapTrampolineRelocInfo(Isolate* isolate);

  // Only builtins with JS linkage should ever need to be called via their
  // trampoline Code object. The remaining builtins have non-executable Code
  // objects.
  static bool CodeObjectIsExecutable(Builtin builtin);

  static bool IsJSEntryVariant(Builtin builtin) {
    switch (builtin) {
      case Builtin::kJSEntry:
      case Builtin::kJSConstructEntry:
      case Builtin::kJSRunMicrotasksEntry:
        return true;
      default:
        return false;
    }
    UNREACHABLE();
  }

  int js_entry_handler_offset() const {
    DCHECK_NE(js_entry_handler_offset_, 0);
    return js_entry_handler_offset_;
  }

  void SetJSEntryHandlerOffset(int offset) {
    // Check the stored offset is either uninitialized or unchanged (we
    // generate multiple variants of this builtin but they should all have the
    // same handler offset).
    CHECK(js_entry_handler_offset_ == 0 || js_entry_handler_offset_ == offset);
    js_entry_handler_offset_ = offset;
  }

  // Returns given builtin's slot in the main builtin table.
  FullObjectSlot builtin_slot(Builtin builtin);
  // Returns given builtin's slot in the tier0 builtin table.
  FullObjectSlot builtin_tier0_slot(Builtin builtin);

 private:
  static void Generate_CallFunction(MacroAssembler* masm,
                                    ConvertReceiverMode mode);

  static void Generate_CallBoundFunctionImpl(MacroAssembler* masm);

  static void Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode);

  enum class CallOrConstructMode { kCall, kConstruct };
  static void Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                              Handle<CodeT> code);
  static void Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                     CallOrConstructMode mode,
                                                     Handle<CodeT> code);

  static void Generate_InterpreterPushArgsThenCallImpl(
      MacroAssembler* masm, ConvertReceiverMode receiver_mode,
      InterpreterPushArgsMode mode);

  static void Generate_InterpreterPushArgsThenConstructImpl(
      MacroAssembler* masm, InterpreterPushArgsMode mode);

#define DECLARE_ASM(Name, ...) \
  static void Generate_##Name(MacroAssembler* masm);
#define DECLARE_TF(Name, ...) \
  static void Generate_##Name(compiler::CodeAssemblerState* state);

  BUILTIN_LIST(IGNORE_BUILTIN, DECLARE_TF, DECLARE_TF, DECLARE_TF, DECLARE_TF,
               IGNORE_BUILTIN, DECLARE_ASM)

#undef DECLARE_ASM
#undef DECLARE_TF

  Isolate* isolate_;
  bool initialized_ = false;

  // Stores the offset of exception handler entry point (the handler_entry
  // label) in JSEntry and its variants. It's used to generate the handler table
  // during codegen (mksnapshot-only).
  int js_entry_handler_offset_ = 0;

  friend class SetupIsolateDelegate;
};

V8_INLINE constexpr bool IsInterpreterTrampolineBuiltin(Builtin builtin_id) {
  // Check for kNoBuiltinId first to abort early when the current Code object
  // is not a builtin.
  return builtin_id != Builtin::kNoBuiltinId &&
         (builtin_id == Builtin::kInterpreterEntryTrampoline ||
          builtin_id == Builtin::kInterpreterEnterAtBytecode ||
          builtin_id == Builtin::kInterpreterEnterAtNextBytecode);
}

V8_INLINE constexpr bool IsBaselineTrampolineBuiltin(Builtin builtin_id) {
  // Check for kNoBuiltinId first to abort early when the current Code object
  // is not a builtin.
  return builtin_id != Builtin::kNoBuiltinId &&
         (builtin_id == Builtin::kBaselineOutOfLinePrologue ||
          builtin_id == Builtin::kBaselineOrInterpreterEnterAtBytecode ||
          builtin_id == Builtin::kBaselineOrInterpreterEnterAtNextBytecode);
}

Builtin ExampleBuiltinForTorqueFunctionPointerType(
    size_t function_pointer_type_id);

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_H_
