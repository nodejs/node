// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/profile-data-reader.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/compiler/code-assembler.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"
#include "src/init/setup-isolate.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-generator.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Address Builtin_##Name(int argc, Address* args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)
#undef FORWARD_DECLARE

namespace {

const int kBufferSize = 128 * KB;

AssemblerOptions BuiltinAssemblerOptions(Isolate* isolate, Builtin builtin) {
  AssemblerOptions options = AssemblerOptions::Default(isolate);
  CHECK(!options.isolate_independent_code);
  CHECK(!options.use_pc_relative_calls_and_jumps);
  CHECK(!options.collect_win64_unwind_info);

  if (!isolate->IsGeneratingEmbeddedBuiltins()) {
    return options;
  }

  const base::AddressRegion& code_region = isolate->heap()->code_region();
  bool pc_relative_calls_fit_in_code_range =
      !code_region.is_empty() &&
      std::ceil(static_cast<float>(code_region.size() / MB)) <=
          kMaxPCRelativeCodeRangeInMB;

  options.isolate_independent_code = true;
  options.use_pc_relative_calls_and_jumps = pc_relative_calls_fit_in_code_range;
  options.collect_win64_unwind_info = true;

  return options;
}

using MacroAssemblerGenerator = void (*)(MacroAssembler*);
using CodeAssemblerGenerator = void (*)(compiler::CodeAssemblerState*);

Handle<Code> BuildPlaceholder(Isolate* isolate, Builtin builtin) {
  HandleScope scope(isolate);
  byte buffer[kBufferSize];
  MacroAssembler masm(isolate, CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, kBufferSize));
  DCHECK(!masm.has_frame());
  {
    FrameScope scope(&masm, StackFrame::NONE);
    // The contents of placeholder don't matter, as long as they don't create
    // embedded constants or external references.
    masm.Move(kJavaScriptCallCodeStartRegister, Smi::zero());
    masm.Call(kJavaScriptCallCodeStartRegister);
  }
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, CodeKind::BUILTIN)
                          .set_self_reference(masm.CodeObject())
                          .set_builtin(builtin)
                          .Build();
  return scope.CloseAndEscape(code);
}

Code BuildWithMacroAssembler(Isolate* isolate, Builtin builtin,
                             MacroAssemblerGenerator generator,
                             const char* s_name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  byte buffer[kBufferSize];

  MacroAssembler masm(isolate, BuiltinAssemblerOptions(isolate, builtin),
                      CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, kBufferSize));
  masm.set_builtin(builtin);
  DCHECK(!masm.has_frame());
  masm.CodeEntry();
  generator(&masm);

  int handler_table_offset = 0;

  // JSEntry builtins are a special case and need to generate a handler table.
  DCHECK_EQ(Builtins::KindOf(Builtin::kJSEntry), Builtins::ASM);
  DCHECK_EQ(Builtins::KindOf(Builtin::kJSConstructEntry), Builtins::ASM);
  DCHECK_EQ(Builtins::KindOf(Builtin::kJSRunMicrotasksEntry), Builtins::ASM);
  if (Builtins::IsJSEntryVariant(builtin)) {
    handler_table_offset = HandlerTable::EmitReturnTableStart(&masm);
    HandlerTable::EmitReturnEntry(
        &masm, 0, isolate->builtins()->js_entry_handler_offset());
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc, MacroAssembler::kNoSafepointTable,
               handler_table_offset);

  Handle<Code> code = Factory::CodeBuilder(isolate, desc, CodeKind::BUILTIN)
                          .set_self_reference(masm.CodeObject())
                          .set_builtin(builtin)
                          .Build();
#if defined(V8_OS_WIN64)
  isolate->SetBuiltinUnwindData(builtin, masm.GetUnwindInfo());
#endif  // V8_OS_WIN64
  return *code;
}

Code BuildAdaptor(Isolate* isolate, Builtin builtin, Address builtin_address,
                  const char* name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  byte buffer[kBufferSize];
  MacroAssembler masm(isolate, BuiltinAssemblerOptions(isolate, builtin),
                      CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, kBufferSize));
  masm.set_builtin(builtin);
  DCHECK(!masm.has_frame());
  Builtins::Generate_Adaptor(&masm, builtin_address);
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, CodeKind::BUILTIN)
                          .set_self_reference(masm.CodeObject())
                          .set_builtin(builtin)
                          .Build();
  return *code;
}

// Builder for builtins implemented in TurboFan with JS linkage.
Code BuildWithCodeStubAssemblerJS(Isolate* isolate, Builtin builtin,
                                  CodeAssemblerGenerator generator, int argc,
                                  const char* name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);

  Zone zone(isolate->allocator(), ZONE_NAME, kCompressGraphZone);
  const int argc_with_recv =
      (argc == kDontAdaptArgumentsSentinel) ? 0 : argc + 1;
  compiler::CodeAssemblerState state(
      isolate, &zone, argc_with_recv, CodeKind::BUILTIN, name,
      PoisoningMitigationLevel::kDontPoison, builtin);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(
      &state, BuiltinAssemblerOptions(isolate, builtin),
      ProfileDataFromFile::TryRead(name));
  return *code;
}

// Builder for builtins implemented in TurboFan with CallStub linkage.
Code BuildWithCodeStubAssemblerCS(Isolate* isolate, Builtin builtin,
                                  CodeAssemblerGenerator generator,
                                  CallDescriptors::Key interface_descriptor,
                                  const char* name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  Zone zone(isolate->allocator(), ZONE_NAME, kCompressGraphZone);
  // The interface descriptor with given key must be initialized at this point
  // and this construction just queries the details from the descriptors table.
  CallInterfaceDescriptor descriptor(interface_descriptor);
  // Ensure descriptor is already initialized.
  DCHECK_LE(0, descriptor.GetRegisterParameterCount());
  compiler::CodeAssemblerState state(
      isolate, &zone, descriptor, CodeKind::BUILTIN, name,
      PoisoningMitigationLevel::kDontPoison, builtin);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(
      &state, BuiltinAssemblerOptions(isolate, builtin),
      ProfileDataFromFile::TryRead(name));
  return *code;
}

}  // anonymous namespace

// static
void SetupIsolateDelegate::AddBuiltin(Builtins* builtins, Builtin builtin,
                                      Code code) {
  DCHECK_EQ(builtin, code.builtin_id());
  builtins->set_code(builtin, code);
}

// static
void SetupIsolateDelegate::PopulateWithPlaceholders(Isolate* isolate) {
  // Fill the builtins list with placeholders. References to these placeholder
  // builtins are eventually replaced by the actual builtins. This is to
  // support circular references between builtins.
  Builtins* builtins = isolate->builtins();
  HandleScope scope(isolate);
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    Handle<Code> placeholder = BuildPlaceholder(isolate, builtin);
    AddBuiltin(builtins, builtin, *placeholder);
  }
}

// static
void SetupIsolateDelegate::ReplacePlaceholders(Isolate* isolate) {
  // Replace references from all builtin code objects to placeholders.
  Builtins* builtins = isolate->builtins();
  DisallowGarbageCollection no_gc;
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  static const int kRelocMask =
      RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
      RelocInfo::ModeMask(RelocInfo::FULL_EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::COMPRESSED_EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET);
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    Code code = builtins->code(builtin);
    bool flush_icache = false;
    for (RelocIterator it(code, kRelocMask); !it.done(); it.next()) {
      RelocInfo* rinfo = it.rinfo();
      if (RelocInfo::IsCodeTargetMode(rinfo->rmode())) {
        Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
        DCHECK_IMPLIES(RelocInfo::IsRelativeCodeTarget(rinfo->rmode()),
                       Builtins::IsIsolateIndependent(target.builtin_id()));
        if (!target.is_builtin()) continue;
        Code new_target = builtins->code(target.builtin_id());
        rinfo->set_target_address(new_target.raw_instruction_start(),
                                  UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
      } else {
        DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
        Object object = rinfo->target_object();
        if (!object.IsCode()) continue;
        Code target = Code::cast(object);
        if (!target.is_builtin()) continue;
        Code new_target = builtins->code(target.builtin_id());
        rinfo->set_target_object(isolate->heap(), new_target,
                                 UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
      }
      flush_icache = true;
    }
    if (flush_icache) {
      FlushInstructionCache(code.raw_instruction_start(),
                            code.raw_instruction_size());
    }
  }
}

namespace {

Code GenerateBytecodeHandler(Isolate* isolate, Builtin builtin,
                             interpreter::OperandScale operand_scale,
                             interpreter::Bytecode bytecode) {
  DCHECK(interpreter::Bytecodes::BytecodeHasHandler(bytecode, operand_scale));
  Handle<Code> code = interpreter::GenerateBytecodeHandler(
      isolate, Builtins::name(builtin), bytecode, operand_scale, builtin,
      BuiltinAssemblerOptions(isolate, builtin));
  return *code;
}

}  // namespace

// static
void SetupIsolateDelegate::SetupBuiltinsInternal(Isolate* isolate) {
  Builtins* builtins = isolate->builtins();
  DCHECK(!builtins->initialized_);

  PopulateWithPlaceholders(isolate);

  // Create a scope for the handles in the builtins.
  HandleScope scope(isolate);

  int index = 0;
  Code code;
#define BUILD_CPP(Name)                                      \
  code = BuildAdaptor(isolate, Builtin::k##Name,             \
                      FUNCTION_ADDR(Builtin_##Name), #Name); \
  AddBuiltin(builtins, Builtin::k##Name, code);              \
  index++;

#define BUILD_TFJ(Name, Argc, ...)                                         \
  code = BuildWithCodeStubAssemblerJS(                                     \
      isolate, Builtin::k##Name, &Builtins::Generate_##Name, Argc, #Name); \
  AddBuiltin(builtins, Builtin::k##Name, code);                            \
  index++;

#define BUILD_TFC(Name, InterfaceDescriptor)                      \
  /* Return size is from the provided CallInterfaceDescriptor. */ \
  code = BuildWithCodeStubAssemblerCS(                            \
      isolate, Builtin::k##Name, &Builtins::Generate_##Name,      \
      CallDescriptors::InterfaceDescriptor, #Name);               \
  AddBuiltin(builtins, Builtin::k##Name, code);                   \
  index++;

#define BUILD_TFS(Name, ...)                                            \
  /* Return size for generic TF builtins (stub linkage) is always 1. */ \
  code = BuildWithCodeStubAssemblerCS(isolate, Builtin::k##Name,        \
                                      &Builtins::Generate_##Name,       \
                                      CallDescriptors::Name, #Name);    \
  AddBuiltin(builtins, Builtin::k##Name, code);                         \
  index++;

#define BUILD_TFH(Name, InterfaceDescriptor)                 \
  /* Return size for IC builtins/handlers is always 1. */    \
  code = BuildWithCodeStubAssemblerCS(                       \
      isolate, Builtin::k##Name, &Builtins::Generate_##Name, \
      CallDescriptors::InterfaceDescriptor, #Name);          \
  AddBuiltin(builtins, Builtin::k##Name, code);              \
  index++;

#define BUILD_BCH(Name, OperandScale, Bytecode)                           \
  code = GenerateBytecodeHandler(isolate, Builtin::k##Name, OperandScale, \
                                 Bytecode);                               \
  AddBuiltin(builtins, Builtin::k##Name, code);                           \
  index++;

#define BUILD_ASM(Name, InterfaceDescriptor)                        \
  code = BuildWithMacroAssembler(isolate, Builtin::k##Name,         \
                                 Builtins::Generate_##Name, #Name); \
  AddBuiltin(builtins, Builtin::k##Name, code);                     \
  index++;

  BUILTIN_LIST(BUILD_CPP, BUILD_TFJ, BUILD_TFC, BUILD_TFS, BUILD_TFH, BUILD_BCH,
               BUILD_ASM);

#undef BUILD_CPP
#undef BUILD_TFJ
#undef BUILD_TFC
#undef BUILD_TFS
#undef BUILD_TFH
#undef BUILD_BCH
#undef BUILD_ASM
  CHECK_EQ(Builtins::kBuiltinCount, index);

  ReplacePlaceholders(isolate);

#define SET_PROMISE_REJECTION_PREDICTION(Name) \
  builtins->code(Builtin::k##Name).set_is_promise_rejection(true);

  BUILTIN_PROMISE_REJECTION_PREDICTION_LIST(SET_PROMISE_REJECTION_PREDICTION)
#undef SET_PROMISE_REJECTION_PREDICTION

#define SET_EXCEPTION_CAUGHT_PREDICTION(Name) \
  builtins->code(Builtin::k##Name).set_is_exception_caught(true);

  BUILTIN_EXCEPTION_CAUGHT_PREDICTION_LIST(SET_EXCEPTION_CAUGHT_PREDICTION)
#undef SET_EXCEPTION_CAUGHT_PREDICTION

  builtins->MarkInitialized();
}

}  // namespace internal
}  // namespace v8
