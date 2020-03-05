// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"

#include "src/api/api-inl.h"
#include "src/builtins/builtins-descriptors.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/codegen/macro-assembler.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/isolate.h"
#include "src/interpreter/bytecodes.h"
#include "src/logging/code-events.h"  // For CodeCreateEvent.
#include "src/logging/log.h"          // For Logger.
#include "src/objects/fixed-array.h"
#include "src/objects/objects-inl.h"
#include "src/objects/visitors.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Address Builtin_##Name(int argc, Address* args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)
#undef FORWARD_DECLARE

namespace {

// TODO(jgruber): Pack in CallDescriptors::Key.
struct BuiltinMetadata {
  const char* name;
  Builtins::Kind kind;

  struct BytecodeAndScale {
    interpreter::Bytecode bytecode : 8;
    interpreter::OperandScale scale : 8;
  };

  STATIC_ASSERT(sizeof(interpreter::Bytecode) == 1);
  STATIC_ASSERT(sizeof(interpreter::OperandScale) == 1);
  STATIC_ASSERT(sizeof(BytecodeAndScale) <= sizeof(Address));

  // The `data` field has kind-specific contents.
  union KindSpecificData {
    // TODO(jgruber): Union constructors are needed since C++11 does not support
    // designated initializers (e.g.: {.parameter_count = count}). Update once
    // we're at C++20 :)
    // The constructors are marked constexpr to avoid the need for a static
    // initializer for builtins.cc (see check-static-initializers.sh).
    constexpr KindSpecificData() : cpp_entry(kNullAddress) {}
    constexpr KindSpecificData(Address cpp_entry) : cpp_entry(cpp_entry) {}
    constexpr KindSpecificData(int parameter_count,
                               int /* To disambiguate from above */)
        : parameter_count(static_cast<int16_t>(parameter_count)) {}
    constexpr KindSpecificData(interpreter::Bytecode bytecode,
                               interpreter::OperandScale scale)
        : bytecode_and_scale{bytecode, scale} {}
    Address cpp_entry;                    // For CPP builtins.
    int16_t parameter_count;              // For TFJ builtins.
    BytecodeAndScale bytecode_and_scale;  // For BCH builtins.
  } data;
};

#define DECL_CPP(Name, ...) \
  {#Name, Builtins::CPP, {FUNCTION_ADDR(Builtin_##Name)}},
#define DECL_TFJ(Name, Count, ...) {#Name, Builtins::TFJ, {Count, 0}},
#define DECL_TFC(Name, ...) {#Name, Builtins::TFC, {}},
#define DECL_TFS(Name, ...) {#Name, Builtins::TFS, {}},
#define DECL_TFH(Name, ...) {#Name, Builtins::TFH, {}},
#define DECL_BCH(Name, OperandScale, Bytecode) \
  {#Name, Builtins::BCH, {Bytecode, OperandScale}},
#define DECL_ASM(Name, ...) {#Name, Builtins::ASM, {}},
const BuiltinMetadata builtin_metadata[] = {BUILTIN_LIST(
    DECL_CPP, DECL_TFJ, DECL_TFC, DECL_TFS, DECL_TFH, DECL_BCH, DECL_ASM)};
#undef DECL_CPP
#undef DECL_TFJ
#undef DECL_TFC
#undef DECL_TFS
#undef DECL_TFH
#undef DECL_BCH
#undef DECL_ASM

}  // namespace

BailoutId Builtins::GetContinuationBailoutId(Name name) {
  DCHECK(Builtins::KindOf(name) == TFJ || Builtins::KindOf(name) == TFC ||
         Builtins::KindOf(name) == TFS);
  return BailoutId(BailoutId::kFirstBuiltinContinuationId + name);
}

Builtins::Name Builtins::GetBuiltinFromBailoutId(BailoutId id) {
  int builtin_index = id.ToInt() - BailoutId::kFirstBuiltinContinuationId;
  DCHECK(Builtins::KindOf(builtin_index) == TFJ ||
         Builtins::KindOf(builtin_index) == TFC ||
         Builtins::KindOf(builtin_index) == TFS);
  return static_cast<Name>(builtin_index);
}

void Builtins::TearDown() { initialized_ = false; }

const char* Builtins::Lookup(Address pc) {
  // Off-heap pc's can be looked up through binary search.
  Code maybe_builtin = InstructionStream::TryLookupCode(isolate_, pc);
  if (!maybe_builtin.is_null()) return name(maybe_builtin.builtin_index());

  // May be called during initialization (disassembler).
  if (initialized_) {
    for (int i = 0; i < builtin_count; i++) {
      if (isolate_->heap()->builtin(i).contains(pc)) return name(i);
    }
  }
  return nullptr;
}

Handle<Code> Builtins::NonPrimitiveToPrimitive(ToPrimitiveHint hint) {
  switch (hint) {
    case ToPrimitiveHint::kDefault:
      return builtin_handle(kNonPrimitiveToPrimitive_Default);
    case ToPrimitiveHint::kNumber:
      return builtin_handle(kNonPrimitiveToPrimitive_Number);
    case ToPrimitiveHint::kString:
      return builtin_handle(kNonPrimitiveToPrimitive_String);
  }
  UNREACHABLE();
}

Handle<Code> Builtins::OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint) {
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      return builtin_handle(kOrdinaryToPrimitive_Number);
    case OrdinaryToPrimitiveHint::kString:
      return builtin_handle(kOrdinaryToPrimitive_String);
  }
  UNREACHABLE();
}

void Builtins::set_builtin(int index, Code builtin) {
  isolate_->heap()->set_builtin(index, builtin);
}

Code Builtins::builtin(int index) { return isolate_->heap()->builtin(index); }

Handle<Code> Builtins::builtin_handle(int index) {
  DCHECK(IsBuiltinId(index));
  return Handle<Code>(
      reinterpret_cast<Address*>(isolate_->heap()->builtin_address(index)));
}

// static
int Builtins::GetStackParameterCount(Name name) {
  DCHECK(Builtins::KindOf(name) == TFJ);
  return builtin_metadata[name].data.parameter_count;
}

// static
CallInterfaceDescriptor Builtins::CallInterfaceDescriptorFor(Name name) {
  CallDescriptors::Key key;
  switch (name) {
// This macro is deliberately crafted so as to emit very little code,
// in order to keep binary size of this function under control.
#define CASE_OTHER(Name, ...)                          \
  case k##Name: {                                      \
    key = Builtin_##Name##_InterfaceDescriptor::key(); \
    break;                                             \
  }
    BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, CASE_OTHER, CASE_OTHER,
                 CASE_OTHER, IGNORE_BUILTIN, CASE_OTHER)
#undef CASE_OTHER
    default:
      Builtins::Kind kind = Builtins::KindOf(name);
      DCHECK_NE(BCH, kind);
      if (kind == TFJ || kind == CPP) {
        return JSTrampolineDescriptor{};
      }
      UNREACHABLE();
  }
  return CallInterfaceDescriptor{key};
}

// static
Callable Builtins::CallableFor(Isolate* isolate, Name name) {
  Handle<Code> code = isolate->builtins()->builtin_handle(name);
  return Callable{code, CallInterfaceDescriptorFor(name)};
}

// static
bool Builtins::HasJSLinkage(int builtin_index) {
  Name name = static_cast<Name>(builtin_index);
  DCHECK_NE(BCH, Builtins::KindOf(name));
  return CallInterfaceDescriptorFor(name) == JSTrampolineDescriptor{};
}

// static
const char* Builtins::name(int index) {
  DCHECK(IsBuiltinId(index));
  return builtin_metadata[index].name;
}

void Builtins::PrintBuiltinCode() {
  DCHECK(FLAG_print_builtin_code);
#ifdef ENABLE_DISASSEMBLER
  for (int i = 0; i < builtin_count; i++) {
    const char* builtin_name = name(i);
    Handle<Code> code = builtin_handle(i);
    if (PassesFilter(CStrVector(builtin_name),
                     CStrVector(FLAG_print_builtin_code_filter))) {
      CodeTracer::Scope trace_scope(isolate_->GetCodeTracer());
      OFStream os(trace_scope.file());
      code->Disassemble(builtin_name, os, isolate_);
      os << "\n";
    }
  }
#endif
}

void Builtins::PrintBuiltinSize() {
  DCHECK(FLAG_print_builtin_size);
  for (int i = 0; i < builtin_count; i++) {
    const char* builtin_name = name(i);
    const char* kind = KindNameOf(i);
    Code code = builtin(i);
    PrintF(stdout, "%s Builtin, %s, %d\n", kind, builtin_name,
           code.InstructionSize());
  }
}

// static
Address Builtins::CppEntryOf(int index) {
  DCHECK(Builtins::IsCpp(index));
  return builtin_metadata[index].data.cpp_entry;
}

// static
bool Builtins::IsBuiltin(const Code code) {
  return Builtins::IsBuiltinId(code.builtin_index());
}

bool Builtins::IsBuiltinHandle(Handle<HeapObject> maybe_code,
                               int* index) const {
  Heap* heap = isolate_->heap();
  Address handle_location = maybe_code.address();
  Address start = heap->builtin_address(0);
  Address end = heap->builtin_address(Builtins::builtin_count);
  if (handle_location >= end) return false;
  if (handle_location < start) return false;
  *index = static_cast<int>(handle_location - start) >> kSystemPointerSizeLog2;
  DCHECK(Builtins::IsBuiltinId(*index));
  return true;
}

// static
bool Builtins::IsIsolateIndependentBuiltin(const Code code) {
  const int builtin_index = code.builtin_index();
  return Builtins::IsBuiltinId(builtin_index) &&
         Builtins::IsIsolateIndependent(builtin_index);
}

// static
void Builtins::InitializeBuiltinEntryTable(Isolate* isolate) {
  EmbeddedData d = EmbeddedData::FromBlob();
  Address* builtin_entry_table = isolate->builtin_entry_table();
  for (int i = 0; i < builtin_count; i++) {
    // TODO(jgruber,chromium:1020986): Remove the CHECK once the linked issue is
    // resolved.
    CHECK(Builtins::IsBuiltinId(isolate->heap()->builtin(i).builtin_index()));
    DCHECK(isolate->heap()->builtin(i).is_off_heap_trampoline());
    builtin_entry_table[i] = d.InstructionStartOfBuiltin(i);
  }
}

// static
void Builtins::EmitCodeCreateEvents(Isolate* isolate) {
  if (!isolate->logger()->is_listening_to_code_events() &&
      !isolate->is_profiling()) {
    return;  // No need to iterate the entire table in this case.
  }

  Address* builtins = isolate->builtins_table();
  int i = 0;
  HandleScope scope(isolate);
  for (; i < kFirstBytecodeHandler; i++) {
    Handle<AbstractCode> code(AbstractCode::cast(Object(builtins[i])), isolate);
    PROFILE(isolate, CodeCreateEvent(CodeEventListener::BUILTIN_TAG, code,
                                     Builtins::name(i)));
  }

  STATIC_ASSERT(kLastBytecodeHandlerPlusOne == builtin_count);
  for (; i < builtin_count; i++) {
    Handle<AbstractCode> code(AbstractCode::cast(Object(builtins[i])), isolate);
    interpreter::Bytecode bytecode =
        builtin_metadata[i].data.bytecode_and_scale.bytecode;
    interpreter::OperandScale scale =
        builtin_metadata[i].data.bytecode_and_scale.scale;
    PROFILE(isolate,
            CodeCreateEvent(
                CodeEventListener::BYTECODE_HANDLER_TAG, code,
                interpreter::Bytecodes::ToString(bytecode, scale).c_str()));
  }
}

namespace {
enum TrampolineType { kAbort, kJump };

class OffHeapTrampolineGenerator {
 public:
  explicit OffHeapTrampolineGenerator(Isolate* isolate)
      : isolate_(isolate),
        masm_(isolate, CodeObjectRequired::kYes,
              ExternalAssemblerBuffer(buffer_, kBufferSize)) {}

  CodeDesc Generate(Address off_heap_entry, TrampolineType type) {
    // Generate replacement code that simply tail-calls the off-heap code.
    DCHECK(!masm_.has_frame());
    {
      FrameScope scope(&masm_, StackFrame::NONE);
      if (type == TrampolineType::kJump) {
        masm_.JumpToInstructionStream(off_heap_entry);
      } else {
        masm_.Trap();
      }
    }

    CodeDesc desc;
    masm_.GetCode(isolate_, &desc);
    return desc;
  }

  Handle<HeapObject> CodeObject() { return masm_.CodeObject(); }

 private:
  Isolate* isolate_;
  // Enough to fit the single jmp.
  static constexpr int kBufferSize = 256;
  byte buffer_[kBufferSize];
  MacroAssembler masm_;
};

constexpr int OffHeapTrampolineGenerator::kBufferSize;

}  // namespace

// static
Handle<Code> Builtins::GenerateOffHeapTrampolineFor(
    Isolate* isolate, Address off_heap_entry, int32_t kind_specfic_flags,
    bool generate_jump_to_instruction_stream) {
  DCHECK_NOT_NULL(isolate->embedded_blob());
  DCHECK_NE(0, isolate->embedded_blob_size());

  OffHeapTrampolineGenerator generator(isolate);

  CodeDesc desc =
      generator.Generate(off_heap_entry, generate_jump_to_instruction_stream
                                             ? TrampolineType::kJump
                                             : TrampolineType::kAbort);

  return Factory::CodeBuilder(isolate, desc, Code::BUILTIN)
      .set_read_only_data_container(kind_specfic_flags)
      .set_self_reference(generator.CodeObject())
      .set_is_executable(generate_jump_to_instruction_stream)
      .Build();
}

// static
Handle<ByteArray> Builtins::GenerateOffHeapTrampolineRelocInfo(
    Isolate* isolate) {
  OffHeapTrampolineGenerator generator(isolate);
  // Generate a jump to a dummy address as we're not actually interested in the
  // generated instruction stream.
  CodeDesc desc = generator.Generate(kNullAddress, TrampolineType::kJump);

  Handle<ByteArray> reloc_info = isolate->factory()->NewByteArray(
      desc.reloc_size, AllocationType::kReadOnly);
  Code::CopyRelocInfoToByteArray(*reloc_info, desc);

  return reloc_info;
}

// static
Builtins::Kind Builtins::KindOf(int index) {
  DCHECK(IsBuiltinId(index));
  return builtin_metadata[index].kind;
}

// static
const char* Builtins::KindNameOf(int index) {
  Kind kind = Builtins::KindOf(index);
  // clang-format off
  switch (kind) {
    case CPP: return "CPP";
    case TFJ: return "TFJ";
    case TFC: return "TFC";
    case TFS: return "TFS";
    case TFH: return "TFH";
    case BCH: return "BCH";
    case ASM: return "ASM";
  }
  // clang-format on
  UNREACHABLE();
}

// static
bool Builtins::IsCpp(int index) { return Builtins::KindOf(index) == CPP; }

// static
bool Builtins::AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                    Handle<JSObject> target_global_proxy) {
  if (FLAG_allow_unsafe_function_constructor) return true;
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  Handle<Context> responsible_context = impl->LastEnteredOrMicrotaskContext();
  // TODO(jochen): Remove this.
  if (responsible_context.is_null()) {
    return true;
  }
  if (*responsible_context == target->context()) return true;
  return isolate->MayAccess(responsible_context, target_global_proxy);
}

// static
bool Builtins::CodeObjectIsExecutable(int builtin_index) {
  // If the runtime/optimized code always knows when executing a given builtin
  // that it is a builtin, then that builtin does not need an executable Code
  // object. Such Code objects can go in read_only_space (and can even be
  // smaller with no branch instruction), thus saving memory.

  // Builtins with JS linkage will always have executable Code objects since
  // they can be called directly from jitted code with no way of determining
  // that they are builtins at generation time. E.g.
  //   f = Array.of;
  //   f(1, 2, 3);
  // TODO(delphick): This is probably too loose but for now Wasm can call any JS
  // linkage builtin via its Code object. Once Wasm is fixed this can either be
  // tighted or removed completely.
  if (Builtins::KindOf(builtin_index) != BCH && HasJSLinkage(builtin_index)) {
    return true;
  }

  // There are some other non-TF builtins that also have JS linkage like
  // InterpreterEntryTrampoline which are explicitly allow-listed below.
  // TODO(delphick): Some of these builtins do not fit with the above, but
  // currently cause problems if they're not executable. This list should be
  // pared down as much as possible.
  switch (builtin_index) {
    case Builtins::kInterpreterEntryTrampoline:
    case Builtins::kCompileLazy:
    case Builtins::kCompileLazyDeoptimizedCode:
    case Builtins::kCallFunction_ReceiverIsNullOrUndefined:
    case Builtins::kCallFunction_ReceiverIsNotNullOrUndefined:
    case Builtins::kCallFunction_ReceiverIsAny:
    case Builtins::kCallBoundFunction:
    case Builtins::kCall_ReceiverIsNullOrUndefined:
    case Builtins::kCall_ReceiverIsNotNullOrUndefined:
    case Builtins::kCall_ReceiverIsAny:
    case Builtins::kArgumentsAdaptorTrampoline:
    case Builtins::kHandleApiCall:
    case Builtins::kInstantiateAsmJs:

    // TODO(delphick): Remove this when calls to it have the trampoline inlined
    // or are converted to use kCallBuiltinPointer.
    case Builtins::kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit:
      return true;
    default:
#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
      // TODO(Loongson): Move non-JS linkage builtins code objects into RO_SPACE
      // caused MIPS platform to crash, and we need some time to handle it. Now
      // disable this change temporarily on MIPS platform.
      return true;
#else
      return false;
#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
  }
}

Builtins::Name ExampleBuiltinForTorqueFunctionPointerType(
    size_t function_pointer_type_id) {
  switch (function_pointer_type_id) {
#define FUNCTION_POINTER_ID_CASE(id, name) \
  case id:                                 \
    return Builtins::k##name;
    TORQUE_FUNCTION_POINTER_TYPE_TO_BUILTIN_MAP(FUNCTION_POINTER_ID_CASE)
#undef FUNCTION_POINTER_ID_CASE
    default:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8
