// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/compiler/code-assembler.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"  // For Heap::code_range.
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

AssemblerOptions BuiltinAssemblerOptions(Isolate* isolate,
                                         int32_t builtin_index) {
  AssemblerOptions options = AssemblerOptions::Default(isolate);
  CHECK(!options.isolate_independent_code);
  CHECK(!options.use_pc_relative_calls_and_jumps);
  CHECK(!options.collect_win64_unwind_info);

  if (!isolate->IsGeneratingEmbeddedBuiltins() ||
      !Builtins::IsIsolateIndependent(builtin_index)) {
    return options;
  }

  const base::AddressRegion& code_range = isolate->heap()->code_range();
  bool pc_relative_calls_fit_in_code_range =
      !code_range.is_empty() &&
      std::ceil(static_cast<float>(code_range.size() / MB)) <=
          kMaxPCRelativeCodeRangeInMB;

  options.isolate_independent_code = true;
  options.use_pc_relative_calls_and_jumps = pc_relative_calls_fit_in_code_range;
  options.collect_win64_unwind_info = true;

  return options;
}

using MacroAssemblerGenerator = void (*)(MacroAssembler*);
using CodeAssemblerGenerator = void (*)(compiler::CodeAssemblerState*);

Handle<Code> BuildPlaceholder(Isolate* isolate, int32_t builtin_index) {
  HandleScope scope(isolate);
  constexpr int kBufferSize = 1 * KB;
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
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::BUILTIN)
                          .set_self_reference(masm.CodeObject())
                          .set_builtin_index(builtin_index)
                          .Build();
  return scope.CloseAndEscape(code);
}

Code BuildWithMacroAssembler(Isolate* isolate, int32_t builtin_index,
                             MacroAssemblerGenerator generator,
                             const char* s_name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  constexpr int kBufferSize = 32 * KB;
  byte buffer[kBufferSize];

  MacroAssembler masm(isolate, BuiltinAssemblerOptions(isolate, builtin_index),
                      CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, kBufferSize));
  masm.set_builtin_index(builtin_index);
  DCHECK(!masm.has_frame());
  masm.CodeEntry();
  generator(&masm);

  int handler_table_offset = 0;

  // JSEntry builtins are a special case and need to generate a handler table.
  DCHECK_EQ(Builtins::KindOf(Builtins::kJSEntry), Builtins::ASM);
  DCHECK_EQ(Builtins::KindOf(Builtins::kJSConstructEntry), Builtins::ASM);
  DCHECK_EQ(Builtins::KindOf(Builtins::kJSRunMicrotasksEntry), Builtins::ASM);
  if (Builtins::IsJSEntryVariant(builtin_index)) {
    handler_table_offset = HandlerTable::EmitReturnTableStart(&masm);
    HandlerTable::EmitReturnEntry(
        &masm, 0, isolate->builtins()->js_entry_handler_offset());
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc, MacroAssembler::kNoSafepointTable,
               handler_table_offset);

  Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::BUILTIN)
                          .set_self_reference(masm.CodeObject())
                          .set_builtin_index(builtin_index)
                          .Build();
#if defined(V8_OS_WIN64)
  isolate->SetBuiltinUnwindData(builtin_index, masm.GetUnwindInfo());
#endif  // V8_OS_WIN64
  return *code;
}

Code BuildAdaptor(Isolate* isolate, int32_t builtin_index,
                  Address builtin_address, const char* name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  constexpr int kBufferSize = 32 * KB;
  byte buffer[kBufferSize];
  MacroAssembler masm(isolate, BuiltinAssemblerOptions(isolate, builtin_index),
                      CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, kBufferSize));
  masm.set_builtin_index(builtin_index);
  DCHECK(!masm.has_frame());
  Builtins::Generate_Adaptor(&masm, builtin_address);
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::BUILTIN)
                          .set_self_reference(masm.CodeObject())
                          .set_builtin_index(builtin_index)
                          .Build();
  return *code;
}

// Builder for builtins implemented in TurboFan with JS linkage.
Code BuildWithCodeStubAssemblerJS(Isolate* isolate, int32_t builtin_index,
                                  CodeAssemblerGenerator generator, int argc,
                                  const char* name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);

  Zone zone(isolate->allocator(), ZONE_NAME);
  const int argc_with_recv =
      (argc == kDontAdaptArgumentsSentinel) ? 0 : argc + 1;
  compiler::CodeAssemblerState state(
      isolate, &zone, argc_with_recv, Code::BUILTIN, name,
      PoisoningMitigationLevel::kDontPoison, builtin_index);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(
      &state, BuiltinAssemblerOptions(isolate, builtin_index));
  return *code;
}

// Builder for builtins implemented in TurboFan with CallStub linkage.
Code BuildWithCodeStubAssemblerCS(Isolate* isolate, int32_t builtin_index,
                                  CodeAssemblerGenerator generator,
                                  CallDescriptors::Key interface_descriptor,
                                  const char* name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  Zone zone(isolate->allocator(), ZONE_NAME);
  // The interface descriptor with given key must be initialized at this point
  // and this construction just queries the details from the descriptors table.
  CallInterfaceDescriptor descriptor(interface_descriptor);
  // Ensure descriptor is already initialized.
  DCHECK_LE(0, descriptor.GetRegisterParameterCount());
  compiler::CodeAssemblerState state(
      isolate, &zone, descriptor, Code::BUILTIN, name,
      PoisoningMitigationLevel::kDontPoison, builtin_index);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(
      &state, BuiltinAssemblerOptions(isolate, builtin_index));
  return *code;
}

}  // anonymous namespace

// static
void SetupIsolateDelegate::AddBuiltin(Builtins* builtins, int index,
                                      Code code) {
  DCHECK_EQ(index, code.builtin_index());
  builtins->set_builtin(index, code);
}

// static
void SetupIsolateDelegate::PopulateWithPlaceholders(Isolate* isolate) {
  // Fill the builtins list with placeholders. References to these placeholder
  // builtins are eventually replaced by the actual builtins. This is to
  // support circular references between builtins.
  Builtins* builtins = isolate->builtins();
  HandleScope scope(isolate);
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Handle<Code> placeholder = BuildPlaceholder(isolate, i);
    AddBuiltin(builtins, i, *placeholder);
  }
}

// static
void SetupIsolateDelegate::ReplacePlaceholders(Isolate* isolate) {
  // Replace references from all code objects to placeholders.
  Builtins* builtins = isolate->builtins();
  DisallowHeapAllocation no_gc;
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  static const int kRelocMask =
      RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
      RelocInfo::ModeMask(RelocInfo::FULL_EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::COMPRESSED_EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET);
  HeapObjectIterator iterator(isolate->heap());
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (!obj.IsCode()) continue;
    Code code = Code::cast(obj);
    bool flush_icache = false;
    for (RelocIterator it(code, kRelocMask); !it.done(); it.next()) {
      RelocInfo* rinfo = it.rinfo();
      if (RelocInfo::IsCodeTargetMode(rinfo->rmode())) {
        Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
        DCHECK_IMPLIES(RelocInfo::IsRelativeCodeTarget(rinfo->rmode()),
                       Builtins::IsIsolateIndependent(target.builtin_index()));
        if (!target.is_builtin()) continue;
        Code new_target = builtins->builtin(target.builtin_index());
        rinfo->set_target_address(new_target.raw_instruction_start(),
                                  UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
      } else {
        DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
        Object object = rinfo->target_object();
        if (!object.IsCode()) continue;
        Code target = Code::cast(object);
        if (!target.is_builtin()) continue;
        Code new_target = builtins->builtin(target.builtin_index());
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

Code GenerateBytecodeHandler(Isolate* isolate, int builtin_index,
                             interpreter::OperandScale operand_scale,
                             interpreter::Bytecode bytecode) {
  DCHECK(interpreter::Bytecodes::BytecodeHasHandler(bytecode, operand_scale));
  Handle<Code> code = interpreter::GenerateBytecodeHandler(
      isolate, Builtins::name(builtin_index), bytecode, operand_scale,
      builtin_index, BuiltinAssemblerOptions(isolate, builtin_index));
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
#define BUILD_CPP(Name)                                                      \
  code = BuildAdaptor(isolate, index, FUNCTION_ADDR(Builtin_##Name), #Name); \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFJ(Name, Argc, ...)                              \
  code = BuildWithCodeStubAssemblerJS(                          \
      isolate, index, &Builtins::Generate_##Name, Argc, #Name); \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFC(Name, InterfaceDescriptor)                      \
  /* Return size is from the provided CallInterfaceDescriptor. */ \
  code = BuildWithCodeStubAssemblerCS(                            \
      isolate, index, &Builtins::Generate_##Name,                 \
      CallDescriptors::InterfaceDescriptor, #Name);               \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFS(Name, ...)                                                   \
  /* Return size for generic TF builtins (stub linkage) is always 1. */        \
  code =                                                                       \
      BuildWithCodeStubAssemblerCS(isolate, index, &Builtins::Generate_##Name, \
                                   CallDescriptors::Name, #Name);              \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFH(Name, InterfaceDescriptor)              \
  /* Return size for IC builtins/handlers is always 1. */ \
  code = BuildWithCodeStubAssemblerCS(                    \
      isolate, index, &Builtins::Generate_##Name,         \
      CallDescriptors::InterfaceDescriptor, #Name);       \
  AddBuiltin(builtins, index++, code);

#define BUILD_BCH(Name, OperandScale, Bytecode)                           \
  code = GenerateBytecodeHandler(isolate, index, OperandScale, Bytecode); \
  AddBuiltin(builtins, index++, code);

#define BUILD_ASM(Name, InterfaceDescriptor)                                \
  code = BuildWithMacroAssembler(isolate, index, Builtins::Generate_##Name, \
                                 #Name);                                    \
  AddBuiltin(builtins, index++, code);

  BUILTIN_LIST(BUILD_CPP, BUILD_TFJ, BUILD_TFC, BUILD_TFS, BUILD_TFH, BUILD_BCH,
               BUILD_ASM);

#undef BUILD_CPP
#undef BUILD_TFJ
#undef BUILD_TFC
#undef BUILD_TFS
#undef BUILD_TFH
#undef BUILD_BCH
#undef BUILD_ASM
  CHECK_EQ(Builtins::builtin_count, index);

  ReplacePlaceholders(isolate);

#define SET_PROMISE_REJECTION_PREDICTION(Name) \
  builtins->builtin(Builtins::k##Name).set_is_promise_rejection(true);

  BUILTIN_PROMISE_REJECTION_PREDICTION_LIST(SET_PROMISE_REJECTION_PREDICTION)
#undef SET_PROMISE_REJECTION_PREDICTION

#define SET_EXCEPTION_CAUGHT_PREDICTION(Name) \
  builtins->builtin(Builtins::k##Name).set_is_exception_caught(true);

  BUILTIN_EXCEPTION_CAUGHT_PREDICTION_LIST(SET_EXCEPTION_CAUGHT_PREDICTION)
#undef SET_EXCEPTION_CAUGHT_PREDICTION

  builtins->MarkInitialized();
}

}  // namespace internal
}  // namespace v8
