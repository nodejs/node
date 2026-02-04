// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"

#include "src/api/api-inl.h"
#include "src/builtins/builtins-descriptors.h"
#include "src/builtins/builtins-inl.h"
#include "src/builtins/data-view-ops.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/isolate.h"
#include "src/heap/combined-heap.h"
#include "src/interpreter/bytecodes.h"
#include "src/logging/code-events.h"  // For CodeCreateEvent.
#include "src/logging/log.h"          // For V8FileLogger.
#include "src/objects/fixed-array.h"
#include "src/objects/objects-inl.h"
#include "src/objects/visitors.h"
#include "src/snapshot/embedded/embedded-data-inl.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name, Argc) \
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

  static_assert(sizeof(interpreter::Bytecode) == 1);
  static_assert(sizeof(interpreter::OperandScale) == 1);
  static_assert(sizeof(BytecodeAndScale) <= sizeof(Address));

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

#define DECL_CPP(Name, Argc) \
  {#Name, Builtins::CPP, {FUNCTION_ADDR(Builtin_##Name)}},
#define DECL_TFJ_TSA(Name, Count, ...) {#Name, Builtins::TFJ_TSA, {Count, 0}},
#define DECL_TFJ(Name, Count, ...) {#Name, Builtins::TFJ, {Count, 0}},
#define DECL_TFC_TSA(Name, ...) {#Name, Builtins::TFC_TSA, {}},
#define DECL_TFC(Name, ...) {#Name, Builtins::TFC, {}},
#define DECL_TFS(Name, ...) {#Name, Builtins::TFS, {}},
#define DECL_TFH(Name, ...) {#Name, Builtins::TFH, {}},
#define DECL_BCH_TSA(Name, OperandScale, Bytecode) \
  {#Name, Builtins::BCH_TSA, {Bytecode, OperandScale}},
#define DECL_BCH(Name, OperandScale, Bytecode) \
  {#Name, Builtins::BCH, {Bytecode, OperandScale}},
#define DECL_ASM(Name, ...) {#Name, Builtins::ASM, {}},
const BuiltinMetadata builtin_metadata[] = {
    BUILTIN_LIST(DECL_CPP, DECL_TFJ_TSA, DECL_TFJ, DECL_TFC_TSA, DECL_TFC,
                 DECL_TFS, DECL_TFH, DECL_BCH_TSA, DECL_BCH, DECL_ASM)};
#undef DECL_CPP
#undef DECL_TFJ_TSA
#undef DECL_TFJ
#undef DECL_TFC_TSA
#undef DECL_TFC
#undef DECL_TFS
#undef DECL_TFH
#undef DECL_BCH_TSA
#undef DECL_BCH
#undef DECL_ASM

}  // namespace

BytecodeOffset Builtins::GetContinuationBytecodeOffset(Builtin builtin) {
  DCHECK(Builtins::KindOf(builtin) == TFJ || Builtins::KindOf(builtin) == TFC ||
         Builtins::KindOf(builtin) == TFS);
  return BytecodeOffset(BytecodeOffset::kFirstBuiltinContinuationId +
                        ToInt(builtin));
}

Builtin Builtins::GetBuiltinFromBytecodeOffset(BytecodeOffset id) {
  Builtin builtin = Builtins::FromInt(
      id.ToInt() - BytecodeOffset::kFirstBuiltinContinuationId);
  DCHECK(Builtins::KindOf(builtin) == TFJ || Builtins::KindOf(builtin) == TFC ||
         Builtins::KindOf(builtin) == TFS);
  return builtin;
}

void Builtins::TearDown() { initialized_ = false; }

const char* Builtins::Lookup(Address pc) {
  // Off-heap pc's can be looked up through binary search.
  Builtin builtin = OffHeapInstructionStream::TryLookupCode(isolate_, pc);
  if (Builtins::IsBuiltinId(builtin)) return name(builtin);

  // May be called during initialization (disassembler).
  if (!initialized_) return nullptr;
  for (Builtin builtin_ix = Builtins::kFirst; builtin_ix <= Builtins::kLast;
       ++builtin_ix) {
    if (code(builtin_ix)->contains(isolate_, pc)) {
      return name(builtin_ix);
    }
  }
  return nullptr;
}

FullObjectSlot Builtins::builtin_slot(Builtin builtin) {
  Address* location = &isolate_->builtin_table()[Builtins::ToInt(builtin)];
  return FullObjectSlot(location);
}

FullObjectSlot Builtins::builtin_tier0_slot(Builtin builtin) {
  DCHECK(IsTier0(builtin));
  Address* location =
      &isolate_->builtin_tier0_table()[Builtins::ToInt(builtin)];
  return FullObjectSlot(location);
}

void Builtins::set_code(Builtin builtin, Tagged<Code> code) {
  DCHECK_EQ(builtin, code->builtin_id());
  DCHECK(Internals::HasHeapObjectTag(code.ptr()));
  // The given builtin may be uninitialized thus we cannot check its type here.
  isolate_->builtin_table()[Builtins::ToInt(builtin)] = code.ptr();
}

Tagged<Code> Builtins::code(Builtin builtin) {
  Address ptr = isolate_->builtin_table()[Builtins::ToInt(builtin)];
  return TrustedCast<Code>(Tagged<Object>(ptr));
}

Handle<Code> Builtins::code_handle(Builtin builtin) {
  Address* location = &isolate_->builtin_table()[Builtins::ToInt(builtin)];
  return Handle<Code>(location);
}

// static
int Builtins::GetStackParameterCount(Builtin builtin) {
  DCHECK(Builtins::KindOf(builtin) == TFJ_TSA ||
         Builtins::KindOf(builtin) == TFJ);
  return builtin_metadata[ToInt(builtin)].data.parameter_count;
}

// static
bool Builtins::CheckFormalParameterCount(
    Builtin builtin, int function_length,
    int formal_parameter_count_with_receiver) {
  DCHECK_LE(0, function_length);
  if (!Builtins::IsBuiltinId(builtin)) {
    return true;
  }

  if (!HasJSLinkage(builtin)) {
    return true;
  }

  // Some special builtins are allowed to be installed on functions with
  // different parameter counts.
  if (builtin == Builtin::kCompileLazy) {
    return true;
  }

  int parameter_count = Builtins::GetFormalParameterCount(builtin);
  return parameter_count == formal_parameter_count_with_receiver;
}

// static
CallInterfaceDescriptor Builtins::CallInterfaceDescriptorFor(Builtin builtin) {
  CallDescriptors::Key key;
  switch (builtin) {
// This macro is deliberately crafted so as to emit very little code,
// in order to keep binary size of this function under control.
#define CASE_OTHER(Name, ...)                          \
  case Builtin::k##Name: {                             \
    key = Builtin_##Name##_InterfaceDescriptor::key(); \
    break;                                             \
  }
    BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, CASE_OTHER,
                 CASE_OTHER, CASE_OTHER, CASE_OTHER, IGNORE_BUILTIN,
                 IGNORE_BUILTIN, CASE_OTHER)
#undef CASE_OTHER
    default:
      Builtins::Kind kind = Builtins::KindOf(builtin);
      if (kind == TFJ_TSA || kind == TFJ || kind == CPP) {
        return JSTrampolineDescriptor{};
      }
      if (kind == BCH || kind == BCH_TSA) {
        return InterpreterDispatchDescriptor{};
      }
      UNREACHABLE();
  }
  return CallInterfaceDescriptor{key};
}

// static
Callable Builtins::CallableFor(Isolate* isolate, Builtin builtin) {
  Handle<Code> code = isolate->builtins()->code_handle(builtin);
  return Callable{code, CallInterfaceDescriptorFor(builtin)};
}

// static
bool Builtins::HasJSLinkage(Builtin builtin) {
  return CallInterfaceDescriptorFor(builtin) == JSTrampolineDescriptor{};
}

// static
const char* Builtins::name(Builtin builtin) {
  int index = ToInt(builtin);
  DCHECK(IsBuiltinId(index));
  return builtin_metadata[index].name;
}

// static
const char* Builtins::NameForStackTrace(Isolate* isolate, Builtin builtin) {
#if V8_ENABLE_WEBASSEMBLY
  // Most builtins are never shown in stack traces. Those that are exposed
  // to JavaScript get their name from the object referring to them. Here
  // we only support a few internal builtins that have special reasons for
  // being shown on stack traces:
  // - builtins that are allowlisted in {StubFrame::Summarize}.
  // - builtins that throw the same error as one of those above, but would
  //   lose information and e.g. print "indexOf" instead of "String.indexOf".
  switch (builtin) {
    case Builtin::kDataViewPrototypeGetBigInt64:
      return "DataView.prototype.getBigInt64";
    case Builtin::kDataViewPrototypeGetBigUint64:
      return "DataView.prototype.getBigUint64";
    case Builtin::kDataViewPrototypeGetFloat16:
      return "DataView.prototype.getFloat16";
    case Builtin::kDataViewPrototypeGetFloat32:
      return "DataView.prototype.getFloat32";
    case Builtin::kDataViewPrototypeGetFloat64:
      return "DataView.prototype.getFloat64";
    case Builtin::kDataViewPrototypeGetInt8:
      return "DataView.prototype.getInt8";
    case Builtin::kDataViewPrototypeGetInt16:
      return "DataView.prototype.getInt16";
    case Builtin::kDataViewPrototypeGetInt32:
      return "DataView.prototype.getInt32";
    case Builtin::kDataViewPrototypeGetUint8:
      return "DataView.prototype.getUint8";
    case Builtin::kDataViewPrototypeGetUint16:
      return "DataView.prototype.getUint16";
    case Builtin::kDataViewPrototypeGetUint32:
      return "DataView.prototype.getUint32";
    case Builtin::kDataViewPrototypeSetBigInt64:
      return "DataView.prototype.setBigInt64";
    case Builtin::kDataViewPrototypeSetBigUint64:
      return "DataView.prototype.setBigUint64";
    case Builtin::kDataViewPrototypeSetFloat16:
      return "DataView.prototype.setFloat16";
    case Builtin::kDataViewPrototypeSetFloat32:
      return "DataView.prototype.setFloat32";
    case Builtin::kDataViewPrototypeSetFloat64:
      return "DataView.prototype.setFloat64";
    case Builtin::kDataViewPrototypeSetInt8:
      return "DataView.prototype.setInt8";
    case Builtin::kDataViewPrototypeSetInt16:
      return "DataView.prototype.setInt16";
    case Builtin::kDataViewPrototypeSetInt32:
      return "DataView.prototype.setInt32";
    case Builtin::kDataViewPrototypeSetUint8:
      return "DataView.prototype.setUint8";
    case Builtin::kDataViewPrototypeSetUint16:
      return "DataView.prototype.setUint16";
    case Builtin::kDataViewPrototypeSetUint32:
      return "DataView.prototype.setUint32";
    case Builtin::kDataViewPrototypeGetByteLength:
      return "get DataView.prototype.byteLength";
    case Builtin::kThrowDataViewDetachedError:
    case Builtin::kThrowDataViewOutOfBounds:
    case Builtin::kThrowDataViewTypeError: {
      DataViewOp op = static_cast<DataViewOp>(isolate->error_message_param());
      return ToString(op);
    }
    case Builtin::kStringPrototypeToLocaleLowerCase:
      return "String.toLocaleLowerCase";
    case Builtin::kStringPrototypeIndexOf:
    case Builtin::kThrowIndexOfCalledOnNull:
      return "String.indexOf";
#if V8_INTL_SUPPORT
    case Builtin::kStringPrototypeToLowerCaseIntl:
#endif
    case Builtin::kThrowToLowerCaseCalledOnNull:
      return "String.toLowerCase";
    case Builtin::kWasmIntToString:
      return "Number.toString";
    default:
      // Callers getting this might well crash, which might be desirable
      // because it's similar to {UNREACHABLE()}, but contrary to that a
      // careful caller can also check the value and use it as an "is a
      // name available for this builtin?" check.
      return nullptr;
  }
#else
  return nullptr;
#endif  // V8_ENABLE_WEBASSEMBLY
}

void Builtins::PrintBuiltinCode() {
  DCHECK(v8_flags.print_builtin_code);
#ifdef ENABLE_DISASSEMBLER
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    const char* builtin_name = name(builtin);
    if (PassesFilter(base::CStrVector(builtin_name),
                     base::CStrVector(v8_flags.print_builtin_code_filter))) {
      CodeTracer::Scope trace_scope(isolate_->GetCodeTracer());
      OFStream os(trace_scope.file());
      Tagged<Code> builtin_code = code(builtin);
      builtin_code->Disassemble(builtin_name, os, isolate_);
      os << "\n";
    }
  }
#endif
}

void Builtins::PrintBuiltinSize() {
  DCHECK(v8_flags.print_builtin_size);
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    const char* builtin_name = name(builtin);
    const char* kind = KindNameOf(builtin);
    Tagged<Code> code = Builtins::code(builtin);
    PrintF(stdout, "%s Builtin, %s, %d\n", kind, builtin_name,
           code->instruction_size());
  }
}

// static
Address Builtins::CppEntryOf(Builtin builtin) {
  DCHECK(Builtins::IsCpp(builtin));
  return builtin_metadata[ToInt(builtin)].data.cpp_entry;
}

Address Builtins::EmbeddedEntryOf(Builtin builtin) {
  static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
  return EmbeddedData::FromBlob().InstructionStartOf(builtin);
}

// static
bool Builtins::IsBuiltin(const Tagged<Code> code) {
  return Builtins::IsBuiltinId(code->builtin_id());
}

bool Builtins::IsBuiltinHandle(IndirectHandle<HeapObject> maybe_code,
                               Builtin* builtin) const {
  Address* handle_location = maybe_code.location();
  Address* builtins_table = isolate_->builtin_table();
  if (handle_location < builtins_table) return false;
  Address* builtins_table_end = &builtins_table[Builtins::kBuiltinCount];
  if (handle_location >= builtins_table_end) return false;
  *builtin = FromInt(static_cast<int>(handle_location - builtins_table));
  return true;
}

// static
bool Builtins::IsIsolateIndependentBuiltin(Tagged<Code> code) {
  Builtin builtin = code->builtin_id();
  return Builtins::IsBuiltinId(builtin) &&
         Builtins::IsIsolateIndependent(builtin);
}

// static
void Builtins::InitializeIsolateDataTables(Isolate* isolate) {
  EmbeddedData embedded_data = EmbeddedData::FromBlob(isolate);
  IsolateData* isolate_data = isolate->isolate_data();

  // The entry table.
  for (Builtin i = Builtins::kFirst; i <= Builtins::kLast; ++i) {
    DCHECK(Builtins::IsBuiltinId(isolate->builtins()->code(i)->builtin_id()));
    DCHECK(!isolate->builtins()->code(i)->has_instruction_stream());
    Builtin builtin_id = i;
#if V8_ENABLE_GEARBOX
    builtin_id = isolate->builtins()->code(i)->builtin_id();
#endif  // V8_ENABLE_GEARBOX
    isolate_data->builtin_entry_table()[ToInt(i)] =
        embedded_data.InstructionStartOf(builtin_id);
  }

  // T0 tables.
  for (Builtin i = Builtins::kFirst; i <= Builtins::kLastTier0; ++i) {
    const int ii = ToInt(i);
    isolate_data->builtin_tier0_entry_table()[ii] =
        isolate_data->builtin_entry_table()[ii];
    isolate_data->builtin_tier0_table()[ii] = isolate_data->builtin_table()[ii];
  }
}

// static
void Builtins::EmitCodeCreateEvents(Isolate* isolate) {
  if (!isolate->IsLoggingCodeCreation()) return;

  Address* builtins = isolate->builtin_table();
  int i = 0;
  HandleScope scope(isolate);
  for (; i < ToInt(Builtin::kFirstBytecodeHandler); i++) {
    auto builtin_code = DirectHandle<Code>::FromSlot(&builtins[i]);
    DirectHandle<AbstractCode> code = Cast<AbstractCode>(builtin_code);
    PROFILE(isolate, CodeCreateEvent(LogEventListener::CodeTag::kBuiltin, code,
                                     Builtins::name(FromInt(i))));
  }

  static_assert(kLastBytecodeHandlerPlusOne == kBuiltinCount);
  for (; i < kBuiltinCount; i++) {
    auto builtin_code = DirectHandle<Code>::FromSlot(&builtins[i]);
    DirectHandle<AbstractCode> code = Cast<AbstractCode>(builtin_code);
    interpreter::Bytecode bytecode =
        builtin_metadata[i].data.bytecode_and_scale.bytecode;
    interpreter::OperandScale scale =
        builtin_metadata[i].data.bytecode_and_scale.scale;
    PROFILE(isolate,
            CodeCreateEvent(
                LogEventListener::CodeTag::kBytecodeHandler, code,
                interpreter::Bytecodes::ToString(bytecode, scale).c_str()));
  }
}

// static
DirectHandle<Code> Builtins::CreateInterpreterEntryTrampolineForProfiling(
    Isolate* isolate) {
  DCHECK_NOT_NULL(isolate->embedded_blob_code());
  DCHECK_NE(0, isolate->embedded_blob_code_size());

  Tagged<Code> code = isolate->builtins()->code(
      Builtin::kInterpreterEntryTrampolineForProfiling);

  CodeDesc desc;
  desc.buffer = reinterpret_cast<uint8_t*>(code->instruction_start());

  int instruction_size = code->instruction_size();
  desc.buffer_size = instruction_size;
  desc.instr_size = instruction_size;

  // Ensure the code doesn't require creation of metadata, otherwise respective
  // fields of CodeDesc should be initialized.
  DCHECK_EQ(code->safepoint_table_size(), 0);
  DCHECK_EQ(code->handler_table_size(), 0);
  DCHECK_EQ(code->constant_pool_size(), 0);
  // TODO(v8:11036): The following DCHECK currently fails if the mksnapshot is
  // run with enabled code comments, i.e. --interpreted_frames_native_stack is
  // incompatible with --code-comments at mksnapshot-time. If ever needed,
  // implement support.
  DCHECK_EQ(code->code_comments_size(), 0);
  DCHECK_EQ(code->unwinding_info_size(), 0);

  desc.safepoint_table_offset = instruction_size;
  desc.handler_table_offset = instruction_size;
  desc.constant_pool_offset = instruction_size;
  desc.code_comments_offset = instruction_size;
  desc.jump_table_info_offset = instruction_size;

  CodeDesc::Verify(&desc);

  return Factory::CodeBuilder(isolate, desc, CodeKind::BUILTIN)
      // Mimic the InterpreterEntryTrampoline.
      .set_builtin(Builtin::kInterpreterEntryTrampoline)
      .Build();
}

Builtins::Kind Builtins::KindOf(Builtin builtin) {
  DCHECK(IsBuiltinId(builtin));
  return builtin_metadata[ToInt(builtin)].kind;
}

// static
const char* Builtins::KindNameOf(Builtin builtin) {
  Kind kind = Builtins::KindOf(builtin);
  // clang-format off
  switch (kind) {
    case CPP: return "CPP";
    case TFJ_TSA: return "TFJ_TSA";
    case TFJ: return "TFJ";
    case TFC_TSA: return "TFC_TSA";
    case TFC: return "TFC";
    case TFS: return "TFS";
    case TFH: return "TFH";
    case BCH_TSA: return "BCH_TSA";
    case BCH: return "BCH";
    case ASM: return "ASM";
  }
  // clang-format on
  UNREACHABLE();
}

// static
bool Builtins::IsCpp(Builtin builtin) {
  return Builtins::KindOf(builtin) == CPP;
}

// static
CodeEntrypointTag Builtins::EntrypointTagFor(Builtin builtin) {
  Kind kind = Builtins::KindOf(builtin);
  switch (kind) {
    case CPP:
    case TFJ_TSA:
    case TFJ:
      return kJSEntrypointTag;
    case BCH_TSA:
    case BCH:
      return kBytecodeHandlerEntrypointTag;
    case TFC:
    case TFC_TSA:
    case TFS:
    case TFH:
    case ASM:
      return CallInterfaceDescriptorFor(builtin).tag();
  }
  UNREACHABLE();
}

// static
CodeSandboxingMode Builtins::SandboxingModeOf(Builtin builtin) {
  Kind kind = Builtins::KindOf(builtin);
  switch (kind) {
    case CPP:
      // CPP builtins are invoked in sandboxed execution mode, but the CEntry
      // trampoline will exit sandboxed mode before calling the actual C++ code.
      // TODO(422994386): investigate running the C++ code in sandboxed mode.
      return CodeSandboxingMode::kSandboxed;
    case TFJ_TSA:
    case TFJ:
      // All builtins with JS linkage run sandboxed.
      return CodeSandboxingMode::kSandboxed;
    case TFH:
    case BCH_TSA:
    case BCH:
      // Bytecode handlers and inline caches run sandboxed.
      return CodeSandboxingMode::kSandboxed;
    case TFS:
      switch (builtin) {
        // Microtask-related builtins run in privileged mode as they need write
        // access to the MicrotaskQueue object.
        case Builtin::kEnqueueMicrotask:
          return CodeSandboxingMode::kUnsandboxed;
        default:
          return CodeSandboxingMode::kSandboxed;
      }
    case TFC_TSA:
    case TFC:
    case ASM:
      return CallInterfaceDescriptorFor(builtin).sandboxing_mode();
  }
}

// static
bool Builtins::AllowDynamicFunction(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<JSObject> target_global_proxy) {
  if (v8_flags.allow_unsafe_function_constructor) return true;
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  DirectHandle<NativeContext> responsible_context = impl->LastEnteredContext();
  // TODO(verwaest): Remove this.
  if (responsible_context.is_null()) {
    return true;
  }
  if (*responsible_context == target->context()) return true;
  return isolate->MayAccess(responsible_context, target_global_proxy);
}

// static
Builtins::JSBuiltinStateFlags Builtins::GetJSBuiltinState(Builtin builtin) {
#ifdef DEBUG
#define CHECK_FEATURE_FLAG_IS_CONSISTENT(IS_ENABLED) \
  static bool is_feature_enabled = IS_ENABLED;       \
  DCHECK_EQ(is_feature_enabled, IS_ENABLED);
#else
#define CHECK_FEATURE_FLAG_IS_CONSISTENT(IS_ENABLED)
#endif  // DEBUG

// Helper macro for returning optional builtin's state depending on whether
// the respective feature is enabled or not.
// In debug mode it also verifies that the state of the feature hasn't changed
// since previous check. This might happen in unit tests if they flip feature
// flags back and forth before Isolate deinitialization.
#define RETURN_FLAG_DEPENDENT_BUILTIN_STATE(IS_FEATURE_ENABLED)               \
  {                                                                           \
    CHECK_FEATURE_FLAG_IS_CONSISTENT(IS_FEATURE_ENABLED);                     \
    return (IS_FEATURE_ENABLED) ? JSBuiltinStateFlag::kEnabledFlagDependent   \
                                : JSBuiltinStateFlag::kDisabledFlagDependent; \
  }

#define RETURN_FLAG_DEPENDENT_LAZY_BUILTIN_STATE(IS_FEATURE_ENABLED) \
  {                                                                  \
    CHECK_FEATURE_FLAG_IS_CONSISTENT(IS_FEATURE_ENABLED);            \
    return (IS_FEATURE_ENABLED)                                      \
               ? JSBuiltinStateFlag::kEnabledFlagDependentLazy       \
               : JSBuiltinStateFlag::kDisabledFlagDependentLazy;     \
  }

  switch (builtin) {
    // Helper builtins with JS calling convention which are not supposed to be
    // called directly from user JS code.
    case Builtin::kConstructFunction:
    case Builtin::kConstructBoundFunction:
    case Builtin::kConstructedNonConstructable:
    case Builtin::kConstructProxy:
    case Builtin::kHandleApiConstruct:
    case Builtin::kArrayConcat:
    case Builtin::kArrayPop:
    case Builtin::kArrayPush:
    case Builtin::kArrayShift:
    case Builtin::kArrayUnshift:
    case Builtin::kFunctionPrototypeBind:
    case Builtin::kInterpreterEntryTrampolineForProfiling:
    // Tiering builtins are set directly into the dispatch table and never
    // via Code object.
    case Builtin::kStartMaglevOptimizeJob:
    case Builtin::kOptimizeMaglevEager:
    case Builtin::kStartTurbofanOptimizeJob:
    case Builtin::kOptimizeTurbofanEager:
    case Builtin::kFunctionLogNextExecution:
    case Builtin::kMarkReoptimizeLazyDeoptimized:
    case Builtin::kMarkLazyDeoptimized:
    // All *DeoptContinuation builtins.
    case Builtin::kArrayEveryLoopEagerDeoptContinuation:
    case Builtin::kArrayEveryLoopLazyDeoptContinuation:
    case Builtin::kArrayFilterLoopEagerDeoptContinuation:
    case Builtin::kArrayFilterLoopLazyDeoptContinuation:
    case Builtin::kArrayFindLoopEagerDeoptContinuation:
    case Builtin::kArrayFindLoopLazyDeoptContinuation:
    case Builtin::kArrayFindLoopAfterCallbackLazyDeoptContinuation:
    case Builtin::kArrayFindIndexLoopEagerDeoptContinuation:
    case Builtin::kArrayFindIndexLoopLazyDeoptContinuation:
    case Builtin::kArrayFindIndexLoopAfterCallbackLazyDeoptContinuation:
    case Builtin::kArrayForEachLoopEagerDeoptContinuation:
    case Builtin::kArrayForEachLoopLazyDeoptContinuation:
    case Builtin::kArrayMapPreLoopLazyDeoptContinuation:
    case Builtin::kArrayMapLoopEagerDeoptContinuation:
    case Builtin::kArrayMapLoopLazyDeoptContinuation:
    case Builtin::kArrayReduceRightPreLoopEagerDeoptContinuation:
    case Builtin::kArrayReduceRightLoopEagerDeoptContinuation:
    case Builtin::kArrayReduceRightLoopLazyDeoptContinuation:
    case Builtin::kArrayReducePreLoopEagerDeoptContinuation:
    case Builtin::kArrayReduceLoopEagerDeoptContinuation:
    case Builtin::kArrayReduceLoopLazyDeoptContinuation:
    case Builtin::kArraySomeLoopEagerDeoptContinuation:
    case Builtin::kArraySomeLoopLazyDeoptContinuation:
    case Builtin::kStringCreateLazyDeoptContinuation:
    case Builtin::kGenericLazyDeoptContinuation:
    case Builtin::kPromiseConstructorLazyDeoptContinuation:
      return JSBuiltinStateFlag::kDisabledJSBuiltin;

    // These builtins with JS calling convention are not JS language builtins
    // but are allowed to be installed into JSFunctions.
    case Builtin::kCompileLazy:
    case Builtin::kDebugBreakTrampoline:
    case Builtin::kHandleApiCallOrConstruct:
    case Builtin::kInstantiateAsmJs:
    case Builtin::kInterpreterEntryTrampoline:
      return JSBuiltinStateFlag::kJSTrampoline;

    // This is rather not a JS trampoline but a crash pad which is allowed
    // to be installed into any JSFunction because it doesn't return.
    case Builtin::kIllegal:
      return JSBuiltinStateFlag::kJSTrampoline;

    // These are core JS builtins which are instantiated lazily.
    case Builtin::kConsoleAssert:
    case Builtin::kArrayFromAsyncIterableOnFulfilled:
    case Builtin::kArrayFromAsyncIterableOnRejected:
    case Builtin::kArrayFromAsyncArrayLikeOnFulfilled:
    case Builtin::kArrayFromAsyncArrayLikeOnRejected:
    case Builtin::kAsyncFromSyncIteratorCloseSyncAndRethrow:
    case Builtin::kAsyncFunctionAwaitRejectClosure:
    case Builtin::kAsyncFunctionAwaitResolveClosure:
    case Builtin::kAsyncDisposableStackOnFulfilled:
    case Builtin::kAsyncDisposableStackOnRejected:
    case Builtin::kAsyncDisposeFromSyncDispose:
    case Builtin::kAsyncGeneratorAwaitResolveClosure:
    case Builtin::kAsyncGeneratorAwaitRejectClosure:
    case Builtin::kAsyncGeneratorYieldWithAwaitResolveClosure:
    case Builtin::kAsyncGeneratorReturnClosedResolveClosure:
    case Builtin::kAsyncGeneratorReturnClosedRejectClosure:
    case Builtin::kAsyncGeneratorReturnResolveClosure:
    case Builtin::kAsyncIteratorPrototypeAsyncDisposeResolveClosure:
    case Builtin::kAsyncIteratorValueUnwrap:
    case Builtin::kCallAsyncModuleFulfilled:
    case Builtin::kCallAsyncModuleRejected:
    case Builtin::kPromiseCapabilityDefaultReject:
    case Builtin::kPromiseCapabilityDefaultResolve:
    case Builtin::kPromiseGetCapabilitiesExecutor:
    case Builtin::kPromiseAllResolveElementClosure:
    case Builtin::kPromiseAllSettledResolveElementClosure:
    case Builtin::kPromiseAllSettledRejectElementClosure:
    case Builtin::kPromiseAnyRejectElementClosure:
    case Builtin::kPromiseValueThunkFinally:
    case Builtin::kPromiseThrowerFinally:
    case Builtin::kPromiseCatchFinally:
    case Builtin::kPromiseThenFinally:
    case Builtin::kProxyRevoke:
      return JSBuiltinStateFlag::kCoreJSLazy;

#if V8_ENABLE_WEBASSEMBLY
    // These builtins with JS calling convention are not JS language builtins
    // but are allowed to be installed into JSFunctions.
    case Builtin::kJSToWasmWrapper:
    case Builtin::kJSToJSWrapper:
    case Builtin::kJSToJSWrapperInvalidSig:
    case Builtin::kWasmPromising:
#if V8_ENABLE_DRUMBRAKE
    case Builtin::kGenericJSToWasmInterpreterWrapper:
#endif
    case Builtin::kWasmStressSwitch:
      return JSBuiltinStateFlag::kJSTrampoline;

    // These are core JS builtins which are instantiated lazily.
    case Builtin::kWasmConstructorWrapper:
    case Builtin::kWasmMethodWrapper:
    case Builtin::kWasmResume:
    case Builtin::kWasmReject:
    // Well known import functions.
    case Builtin::kWebAssemblyStringCast:
    case Builtin::kWebAssemblyStringTest:
    case Builtin::kWebAssemblyStringFromWtf16Array:
    case Builtin::kWebAssemblyStringFromUtf8Array:
    case Builtin::kWebAssemblyStringIntoUtf8Array:
    case Builtin::kWebAssemblyStringToUtf8Array:
    case Builtin::kWebAssemblyStringToWtf16Array:
    case Builtin::kWebAssemblyStringFromCharCode:
    case Builtin::kWebAssemblyStringFromCodePoint:
    case Builtin::kWebAssemblyStringCodePointAt:
    case Builtin::kWebAssemblyStringCharCodeAt:
    case Builtin::kWebAssemblyStringLength:
    case Builtin::kWebAssemblyStringMeasureUtf8:
    case Builtin::kWebAssemblyStringConcat:
    case Builtin::kWebAssemblyStringSubstring:
    case Builtin::kWebAssemblyStringEquals:
    case Builtin::kWebAssemblyStringCompare:
    case Builtin::kWebAssemblyConfigureAllPrototypes:
      return JSBuiltinStateFlag::kCoreJSLazy;
#endif  // V8_ENABLE_WEBASSEMBLY

#ifdef V8_INTL_SUPPORT
    // Some Intl builtins are lazily instantiated.
    case Builtin::kCollatorInternalCompare:
    case Builtin::kDateTimeFormatInternalFormat:
    case Builtin::kNumberFormatInternalFormatNumber:
    case Builtin::kV8BreakIteratorInternalAdoptText:
    case Builtin::kV8BreakIteratorInternalBreakType:
    case Builtin::kV8BreakIteratorInternalCurrent:
    case Builtin::kV8BreakIteratorInternalFirst:
    case Builtin::kV8BreakIteratorInternalNext:
      return JSBuiltinStateFlag::kCoreJSLazy;
#endif  // V8_INTL_SUPPORT

#ifdef V8_TEMPORAL_SUPPORT
#define CASE(Name, ...) case Builtin::k##Name:
      BUILTIN_LIST_TEMPORAL(CASE, CASE)  // CPP, TFJ
#undef CASE
      RETURN_FLAG_DEPENDENT_LAZY_BUILTIN_STATE(v8_flags.harmony_temporal);
#endif  // V8_TEMPORAL_SUPPORT

      //
      // Various feature-dependent builtins.
      //

#if V8_ENABLE_WEBASSEMBLY
    case Builtin::kWebAssemblyFunctionPrototypeBind:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(
          wasm::WasmEnabledFeatures::FromFlags().has_type_reflection());
#endif  // V8_ENABLE_WEBASSEMBLY

    // --enable-experimental-regexp-engine
    case Builtin::kRegExpPrototypeLinearGetter:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(
          v8_flags.enable_experimental_regexp_engine);

    // --js-source-phase-imports
    case Builtin::kIllegalInvocationThrower:
    case Builtin::kAbstractModuleSourceToStringTag:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_source_phase_imports);

    // --harmony-shadow-realm
    case Builtin::kShadowRealmConstructor:
    case Builtin::kShadowRealmPrototypeEvaluate:
    case Builtin::kShadowRealmPrototypeImportValue:
    case Builtin::kShadowRealmImportValueRejected:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.harmony_shadow_realm);
    case Builtin::kShadowRealmImportValueFulfilled:
      RETURN_FLAG_DEPENDENT_LAZY_BUILTIN_STATE(v8_flags.harmony_shadow_realm);

    // --harmony-struct
    case Builtin::kSharedSpaceJSObjectHasInstance:
    case Builtin::kSharedStructTypeConstructor:
    case Builtin::kSharedStructTypeIsSharedStruct:
    case Builtin::kSharedArrayConstructor:
    case Builtin::kSharedArrayIsSharedArray:
    case Builtin::kAtomicsMutexConstructor:
    case Builtin::kAtomicsMutexLock:
    case Builtin::kAtomicsMutexLockWithTimeout:
    case Builtin::kAtomicsMutexTryLock:
    case Builtin::kAtomicsMutexIsMutex:
    case Builtin::kAtomicsConditionConstructor:
    case Builtin::kAtomicsConditionWait:
    case Builtin::kAtomicsConditionNotify:
    case Builtin::kAtomicsConditionIsCondition:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.harmony_struct);
    case Builtin::kSharedStructConstructor:
      RETURN_FLAG_DEPENDENT_LAZY_BUILTIN_STATE(v8_flags.harmony_struct);

    // --js-promise-try
    case Builtin::kPromiseTry:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_promise_try);

    // --js-atomics-pause
    case Builtin::kAtomicsPause:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_atomics_pause);

    // --js-error-iserror
    case Builtin::kErrorIsError:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_error_iserror);

    // --js-regexp-escape
    case Builtin::kRegExpEscape:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_regexp_escape);

    // --js-explicit-resource-management
    case Builtin::kSuppressedErrorConstructor:
    case Builtin::kDisposableStackConstructor:
    case Builtin::kDisposableStackPrototypeUse:
    case Builtin::kDisposableStackPrototypeDispose:
    case Builtin::kDisposableStackPrototypeAdopt:
    case Builtin::kDisposableStackPrototypeDefer:
    case Builtin::kDisposableStackPrototypeMove:
    case Builtin::kDisposableStackPrototypeGetDisposed:
    case Builtin::kAsyncDisposableStackConstructor:
    case Builtin::kAsyncDisposableStackPrototypeUse:
    case Builtin::kAsyncDisposableStackPrototypeDisposeAsync:
    case Builtin::kAsyncDisposableStackPrototypeAdopt:
    case Builtin::kAsyncDisposableStackPrototypeDefer:
    case Builtin::kAsyncDisposableStackPrototypeMove:
    case Builtin::kAsyncDisposableStackPrototypeGetDisposed:
    case Builtin::kIteratorPrototypeDispose:
    case Builtin::kAsyncIteratorPrototypeAsyncDispose:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(
          v8_flags.js_explicit_resource_management);

    // --js-float16array
    case Builtin::kMathF16round:
    case Builtin::kDataViewPrototypeGetFloat16:
    case Builtin::kDataViewPrototypeSetFloat16:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_float16array);

    // --js-base-64
    case Builtin::kUint8ArrayFromBase64:
    case Builtin::kUint8ArrayFromHex:
    case Builtin::kUint8ArrayPrototypeToBase64:
    case Builtin::kUint8ArrayPrototypeSetFromBase64:
    case Builtin::kUint8ArrayPrototypeToHex:
    case Builtin::kUint8ArrayPrototypeSetFromHex:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_base_64);

    // --js-iterator-sequencing:
    case Builtin::kIteratorConcat:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_iterator_sequencing);

    // --js-upsert
    case Builtin::kMapPrototypeGetOrInsert:
    case Builtin::kMapPrototypeGetOrInsertComputed:
    case Builtin::kWeakMapPrototypeGetOrInsert:
    case Builtin::kWeakMapPrototypeGetOrInsertComputed:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_upsert);

    // --js-immutable-arraybuffer
    case Builtin::kArrayBufferPrototypeGetImmutable:
    case Builtin::kArrayBufferPrototypeTransferToImmutable:
    case Builtin::kArrayBufferPrototypeSliceToImmutable:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_immutable_arraybuffer);

#ifdef V8_INTL_SUPPORT
    // --js-intl-locale-variants
    case Builtin::kLocalePrototypeVariants:
      RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.js_intl_locale_variants);
#endif  // V8_INTL_SUPPORT

    default: {
      // Treat all other JS builtins as mandatory core JS language builtins.
      // This will allow us to detect optional builtins (because mandatory JS
      // builtins must be installed somewhere by default) and we allowlist only
      // them in this switch.
      return HasJSLinkage(builtin) ? JSBuiltinStateFlag::kCoreJSMandatory
                                   : JSBuiltinStateFlag::kDisabledNonJSBuiltin;
    }
  }
  UNREACHABLE();

#undef CHECK_FEATURE_FLAG_IS_CONSISTENT
#undef RETURN_FLAG_DEPENDENT_BUILTIN_STATE
#undef RETURN_FLAG_DEPENDENT_LAZY_BUILTIN_STATE
}

#ifdef DEBUG

void Builtins::VerifyGetJSBuiltinState(bool allow_non_initial_state) {
  CombinedHeapObjectIterator iterator(isolate_->heap());

  // JS builtins installed in JSFunctions.
  std::vector<bool> used_js_builtins(
      static_cast<size_t>(Builtins::kBuiltinCount));
  bool js_functions_exist = false;

  // Step 1: iterate the heap and record builtins that are installed in
  // JSFunctions.
  for (Tagged<HeapObject> obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (IsAnyHole(obj)) continue;

    Tagged<JSFunction> func;
    if (!TryCast(obj, &func)) continue;
    js_functions_exist = true;

    Builtin builtin = func->code(isolate_)->builtin_id();
    size_t builtin_id = static_cast<size_t>(builtin);
    if (builtin_id < Builtins::kBuiltinCount) {
      used_js_builtins[builtin_id] = true;
    }
  }
  if (!js_functions_exist) {
    // If there are no JSFunctions in the heap then the isolate instance
    // must have not been initialized yet and thus checking builtins usages
    // doesn't make sense.
    return;
  }

  // Step 2: make sure the results match the GetJSBuiltinState() predicate.
  size_t bad_builtins_count = 0;
  for (Builtin i = Builtins::kFirst; i <= Builtins::kLast; ++i) {
    JSBuiltinStateFlags state = GetJSBuiltinState(i);

    bool is_enabled = state & JSBuiltinStateFlag::kEnabled;
    bool is_flag_dependent = state & JSBuiltinStateFlag::kFlagDependent;
    bool is_lazy = state & JSBuiltinStateFlag::kLazy;
    bool is_core_JS = state & JSBuiltinStateFlag::kCoreJS;
    bool has_JS_linkage = !(state & JSBuiltinStateFlag::kNonJSLinkage);

    // Some sanity checks.
    CHECK_IMPLIES(is_core_JS, is_enabled && !is_flag_dependent);

    const char* error = nullptr;  // No errors yet.
    bool used = used_js_builtins[static_cast<size_t>(i)];

    if (has_JS_linkage != HasJSLinkage(i)) {
      if (has_JS_linkage) {
        error = "non-JS builtin doesn't have kNonJSLinkage flag";
      } else {
        // Incorrectly set kNonJSLinkage flag.
        error = "JS builtin has kNonJSLinkage flag";
      }

    } else if (is_enabled && !has_JS_linkage) {
      // Builtins with non-JS linkage are not allowed to be installed into
      // JSFunctions. Maybe the builtin was incorrectly attributed?
      error = "builtin with non-JS linkage must be disabled";

    } else if (used) {
      // The builtin is installed into some JSFunction.
      if (!is_enabled) {
        // Only enabled builtins are allowed to be used. Possible reasons:
        //  - is this builtin belongs to a feature behind a flag?
        //  - is this a core JS language builtin which is supposed to be
        //    instantiated lazily?
        error = "using disabled builtin";

      } else if (is_lazy && !allow_non_initial_state) {
        // This check is triggered by %VerifyGetJSBuiltinState(false); call
        // and it expects and verifies that none of lazy functions are
        // instantiated. Possible reasons it fails:
        //  - is the builtin was marked as lazy by mistake?
        //  - is %VerifyGetJSBuiltinState(false); called after instantiations
        //    of lazy builtins was triggered by the user code?
        error = "unexpected usage of lazy builtin";
      }

    } else if (is_enabled && !allow_non_initial_state) {
      // If builtin is not used and the builtins state is expected to be in
      // initial state (i.e. user code hasn't monkey-patched things) then
      // we could perform some additional checks.

      if (is_flag_dependent && !is_lazy) {
        // This builtin might have been marked as enabled by mistake:
        //  - is the feature flag used correct?
        //  - is this builtin instantiated lazily?
        error = "non-lazy optional builtin is not used while it was enabled";

      } else if (is_core_JS && !is_lazy) {
        // This builtin might have been marked as mandatory JS language feature
        // by mistake:
        //  - is this builtin instantiated lazily?
        //  - is the builtin a new JS trampoline?
        error = "mandatory JS builtin is not used while it was enabled";
      }
    }

    if (error) {
      if (bad_builtins_count == 0) {
        PrintF(
            "#\n# Builtins::GetJSBuiltinState() predicate is wrong for "
            "the following builtins:\n#\n");
      }
      bad_builtins_count++;
      PrintF("    case Builtin::k%s:  // %s\n", Builtins::name(i), error);
    }
  }
  // If you see this check failing then you must have added a builtin with
  // JS calling convention that's not installed into any JSFunction with
  // the default set of V8 flags.
  // The reasons might be:
  //  a) the builtin has JS calling convention but it's not supposed to be
  //     installed into any JSFunction (for example, ConstructFunction or
  //     various continuation builtins),
  //  b) the builtin is supposed to be installed into JSFunction but it belongs
  //     to an experimental, incomplete or otherwise disabled feature
  //     controlled by a certain runtime flag (for example,
  //     ShadowRealmConstructor),
  //  c) the builtin is supposed to be installed into JSFunction lazily and
  //     it belongs to an experimental, incomplete or otherwise disabled feature
  //     controlled by a certain runtime flag (for example,
  //     SharedStructConstructor),
  //  d) the builtin belongs to a JavaScript language feature but respective
  //     JSFunction instances are created lazily (for example, Temporal
  //     builtins).
  //  e) the builtin belongs a core V8 machinery (such as CompileLazy,
  //     HandleApiCallOrConstruct or similar).
  //
  // To fix this you should make Builtins::GetJSBuiltinState() return the
  // following value for the builtin depending on the case mentioned above:
  //  a) return JSBuiltinStateFlag::kDisabledJSBuiltin;
  //  b) RETURN_FLAG_DEPENDENT_BUILTIN_STATE(v8_flags.the_feature_flag);
  //  c) RETURN_FLAG_DEPENDENT_LAZY_BUILTIN_STATE(v8_flags.the_feature_flag);
  //  d) return JSBuiltinStateFlag::kCoreJSLazy;
  //  e) return JSBuiltinStateFlag::kJSTrampoline;
  //
  // If you are adding builtins for experimental or otherwise disabled feature
  // make sure you add a regression test for that flag too.
  // For example, see:
  //  - test/mjsunit/harmony/shadowrealm-builtins.js
  //  - test/mjsunit/shared-memory/builtins.js
  //
  // This check might also fail for mandatory builtins if the JS code deleted
  // a mandatory builtin function. If that's what the test is expected to do
  // then disable the verification: --no-verify-get-js-builtin-state.
  CHECK_WITH_MSG(bad_builtins_count == 0,
                 "Builtins::GetJSBuiltinState() predicate requires updating");
}

#endif  // DEBUG

Builtin ExampleBuiltinForTorqueFunctionPointerType(
    size_t function_pointer_type_id) {
  switch (function_pointer_type_id) {
#define FUNCTION_POINTER_ID_CASE(id, name) \
  case id:                                 \
    return Builtin::k##name;
    TORQUE_FUNCTION_POINTER_TYPE_TO_BUILTIN_MAP(FUNCTION_POINTER_ID_CASE)
#undef FUNCTION_POINTER_ID_CASE
    default:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8
