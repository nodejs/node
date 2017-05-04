// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

#include "src/builtins/builtins.h"
#include "src/code-events.h"
#include "src/compiler/code-assembler.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Object* Builtin_##Name(int argc, Object** args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)

namespace {
void PostBuildProfileAndTracing(Isolate* isolate, Code* code,
                                const char* name) {
  PROFILE(isolate, CodeCreateEvent(CodeEventListener::BUILTIN_TAG,
                                   AbstractCode::cast(code), name));
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_builtin_code) {
    CodeTracer::Scope trace_scope(isolate->GetCodeTracer());
    OFStream os(trace_scope.file());
    os << "Builtin: " << name << "\n";
    code->Disassemble(name, os);
    os << "\n";
  }
#endif
}

typedef void (*MacroAssemblerGenerator)(MacroAssembler*);
typedef void (*CodeAssemblerGenerator)(compiler::CodeAssemblerState*);

Code* BuildWithMacroAssembler(Isolate* isolate,
                              MacroAssemblerGenerator generator,
                              Code::Flags flags, const char* s_name) {
  HandleScope scope(isolate);
  const size_t buffer_size = 32 * KB;
  byte buffer[buffer_size];  // NOLINT(runtime/arrays)
  MacroAssembler masm(isolate, buffer, buffer_size, CodeObjectRequired::kYes);
  DCHECK(!masm.has_frame());
  generator(&masm);
  CodeDesc desc;
  masm.GetCode(&desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, flags, masm.CodeObject());
  PostBuildProfileAndTracing(isolate, *code, s_name);
  return *code;
}

Code* BuildAdaptor(Isolate* isolate, Address builtin_address,
                   Builtins::ExitFrameType exit_frame_type, Code::Flags flags,
                   const char* name) {
  HandleScope scope(isolate);
  const size_t buffer_size = 32 * KB;
  byte buffer[buffer_size];  // NOLINT(runtime/arrays)
  MacroAssembler masm(isolate, buffer, buffer_size, CodeObjectRequired::kYes);
  DCHECK(!masm.has_frame());
  Builtins::Generate_Adaptor(&masm, builtin_address, exit_frame_type);
  CodeDesc desc;
  masm.GetCode(&desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, flags, masm.CodeObject());
  PostBuildProfileAndTracing(isolate, *code, name);
  return *code;
}

// Builder for builtins implemented in TurboFan with JS linkage.
Code* BuildWithCodeStubAssemblerJS(Isolate* isolate,
                                   CodeAssemblerGenerator generator, int argc,
                                   Code::Flags flags, const char* name) {
  HandleScope scope(isolate);
  Zone zone(isolate->allocator(), ZONE_NAME);
  const int argc_with_recv =
      (argc == SharedFunctionInfo::kDontAdaptArgumentsSentinel) ? 0 : argc + 1;
  compiler::CodeAssemblerState state(isolate, &zone, argc_with_recv, flags,
                                     name);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(&state);
  PostBuildProfileAndTracing(isolate, *code, name);
  return *code;
}

// Builder for builtins implemented in TurboFan with CallStub linkage.
Code* BuildWithCodeStubAssemblerCS(Isolate* isolate,
                                   CodeAssemblerGenerator generator,
                                   CallDescriptors::Key interface_descriptor,
                                   Code::Flags flags, const char* name,
                                   int result_size) {
  HandleScope scope(isolate);
  Zone zone(isolate->allocator(), ZONE_NAME);
  // The interface descriptor with given key must be initialized at this point
  // and this construction just queries the details from the descriptors table.
  CallInterfaceDescriptor descriptor(isolate, interface_descriptor);
  // Ensure descriptor is already initialized.
  DCHECK_LE(0, descriptor.GetRegisterParameterCount());
  compiler::CodeAssemblerState state(isolate, &zone, descriptor, flags, name,
                                     result_size);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(&state);
  PostBuildProfileAndTracing(isolate, *code, name);
  return *code;
}
}  // anonymous namespace

void SetupIsolateDelegate::AddBuiltin(Builtins* builtins, int index,
                                      Code* code) {
  builtins->builtins_[index] = code;
  code->set_builtin_index(index);
}

void SetupIsolateDelegate::SetupBuiltinsInternal(Isolate* isolate) {
  Builtins* builtins = isolate->builtins();
  DCHECK(!builtins->initialized_);

  // Create a scope for the handles in the builtins.
  HandleScope scope(isolate);

  int index = 0;
  const Code::Flags kBuiltinFlags = Code::ComputeFlags(Code::BUILTIN);
  Code* code;
#define BUILD_CPP(Name)                                              \
  code = BuildAdaptor(isolate, FUNCTION_ADDR(Builtin_##Name),        \
                      Builtins::BUILTIN_EXIT, kBuiltinFlags, #Name); \
  AddBuiltin(builtins, index++, code);
#define BUILD_API(Name)                                                       \
  code = BuildAdaptor(isolate, FUNCTION_ADDR(Builtin_##Name), Builtins::EXIT, \
                      kBuiltinFlags, #Name);                                  \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFJ(Name, Argc, ...)                                         \
  code = BuildWithCodeStubAssemblerJS(isolate, &Builtins::Generate_##Name, \
                                      Argc, kBuiltinFlags, #Name);         \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFC(Name, InterfaceDescriptor, result_size)                   \
  { InterfaceDescriptor##Descriptor descriptor(isolate); }                  \
  code = BuildWithCodeStubAssemblerCS(isolate, &Builtins::Generate_##Name,  \
                                      CallDescriptors::InterfaceDescriptor, \
                                      kBuiltinFlags, #Name, result_size);   \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFS(Name, ...)                                                \
  /* Return size for generic TF builtins (stub linkage) is always 1. */     \
  code = BuildWithCodeStubAssemblerCS(isolate, &Builtins::Generate_##Name,  \
                                      CallDescriptors::Name, kBuiltinFlags, \
                                      #Name, 1);                            \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFH(Name, Kind, Extra, InterfaceDescriptor)                    \
  { InterfaceDescriptor##Descriptor descriptor(isolate); }                   \
  /* Return size for IC builtins/handlers is always 1. */                    \
  code = BuildWithCodeStubAssemblerCS(isolate, &Builtins::Generate_##Name,   \
                                      CallDescriptors::InterfaceDescriptor,  \
                                      Code::ComputeFlags(Code::Kind, Extra), \
                                      #Name, 1);                             \
  AddBuiltin(builtins, index++, code);
#define BUILD_ASM(Name)                                              \
  code = BuildWithMacroAssembler(isolate, Builtins::Generate_##Name, \
                                 kBuiltinFlags, #Name);              \
  AddBuiltin(builtins, index++, code);

  BUILTIN_LIST(BUILD_CPP, BUILD_API, BUILD_TFJ, BUILD_TFC, BUILD_TFS, BUILD_TFH,
               BUILD_ASM, BUILD_ASM);

#undef BUILD_CPP
#undef BUILD_API
#undef BUILD_TFJ
#undef BUILD_TFC
#undef BUILD_TFS
#undef BUILD_TFH
#undef BUILD_ASM
  CHECK_EQ(Builtins::builtin_count, index);

#define SET_PROMISE_REJECTION_PREDICTION(Name)       \
  Code::cast(builtins->builtins_[Builtins::k##Name]) \
      ->set_is_promise_rejection(true);

  BUILTIN_PROMISE_REJECTION_PREDICTION_LIST(SET_PROMISE_REJECTION_PREDICTION)
#undef SET_PROMISE_REJECTION_PREDICTION

#define SET_EXCEPTION_CAUGHT_PREDICTION(Name)        \
  Code::cast(builtins->builtins_[Builtins::k##Name]) \
      ->set_is_exception_caught(true);

  BUILTIN_EXCEPTION_CAUGHT_PREDICTION_LIST(SET_EXCEPTION_CAUGHT_PREDICTION)
#undef SET_EXCEPTION_CAUGHT_PREDICTION

#define SET_CODE_NON_TAGGED_PARAMS(Name)             \
  Code::cast(builtins->builtins_[Builtins::k##Name]) \
      ->set_has_tagged_params(false);

  BUILTINS_WITH_UNTAGGED_PARAMS(SET_CODE_NON_TAGGED_PARAMS)
#undef SET_CODE_NON_TAGGED_PARAMS

  isolate->builtins()->MarkInitialized();
}

}  // namespace internal
}  // namespace v8
