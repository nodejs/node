// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/code-events.h"
#include "src/code-stub-assembler.h"
#include "src/ic/ic-state.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Object* Builtin_##Name(int argc, Object** args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)

Builtins::Builtins() : initialized_(false) {
  memset(builtins_, 0, sizeof(builtins_[0]) * builtin_count);
}

Builtins::~Builtins() {}

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
typedef void (*CodeAssemblerGenerator)(CodeStubAssembler*);

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
  Zone zone(isolate->allocator());
  CodeStubAssembler assembler(isolate, &zone, argc, flags, name);
  generator(&assembler);
  Handle<Code> code = assembler.GenerateCode();
  PostBuildProfileAndTracing(isolate, *code, name);
  return *code;
}

// Builder for builtins implemented in TurboFan with CallStub linkage.
Code* BuildWithCodeStubAssemblerCS(Isolate* isolate,
                                   CodeAssemblerGenerator generator,
                                   CallDescriptors::Key interface_descriptor,
                                   Code::Flags flags, const char* name) {
  HandleScope scope(isolate);
  Zone zone(isolate->allocator());
  // The interface descriptor with given key must be initialized at this point
  // and this construction just queries the details from the descriptors table.
  CallInterfaceDescriptor descriptor(isolate, interface_descriptor);
  // Ensure descriptor is already initialized.
  DCHECK_LE(0, descriptor.GetRegisterParameterCount());
  CodeStubAssembler assembler(isolate, &zone, descriptor, flags, name);
  generator(&assembler);
  Handle<Code> code = assembler.GenerateCode();
  PostBuildProfileAndTracing(isolate, *code, name);
  return *code;
}
}  // anonymous namespace

void Builtins::SetUp(Isolate* isolate, bool create_heap_objects) {
  DCHECK(!initialized_);

  // Create a scope for the handles in the builtins.
  HandleScope scope(isolate);

  if (create_heap_objects) {
    int index = 0;
    const Code::Flags kBuiltinFlags = Code::ComputeFlags(Code::BUILTIN);
    Code* code;
#define BUILD_CPP(Name)                                                     \
  code = BuildAdaptor(isolate, FUNCTION_ADDR(Builtin_##Name), BUILTIN_EXIT, \
                      kBuiltinFlags, #Name);                                \
  builtins_[index++] = code;
#define BUILD_API(Name)                                             \
  code = BuildAdaptor(isolate, FUNCTION_ADDR(Builtin_##Name), EXIT, \
                      kBuiltinFlags, #Name);                        \
  builtins_[index++] = code;
#define BUILD_TFJ(Name, Argc)                                          \
  code = BuildWithCodeStubAssemblerJS(isolate, &Generate_##Name, Argc, \
                                      kBuiltinFlags, #Name);           \
  builtins_[index++] = code;
#define BUILD_TFS(Name, Kind, Extra, InterfaceDescriptor)              \
  { InterfaceDescriptor##Descriptor descriptor(isolate); }             \
  code = BuildWithCodeStubAssemblerCS(                                 \
      isolate, &Generate_##Name, CallDescriptors::InterfaceDescriptor, \
      Code::ComputeFlags(Code::Kind, Extra), #Name);                   \
  builtins_[index++] = code;
#define BUILD_ASM(Name)                                                        \
  code =                                                                       \
      BuildWithMacroAssembler(isolate, Generate_##Name, kBuiltinFlags, #Name); \
  builtins_[index++] = code;
#define BUILD_ASH(Name, Kind, Extra)                                           \
  code = BuildWithMacroAssembler(                                              \
      isolate, Generate_##Name, Code::ComputeFlags(Code::Kind, Extra), #Name); \
  builtins_[index++] = code;

    BUILTIN_LIST(BUILD_CPP, BUILD_API, BUILD_TFJ, BUILD_TFS, BUILD_ASM,
                 BUILD_ASH, BUILD_ASM);

#undef BUILD_CPP
#undef BUILD_API
#undef BUILD_TFJ
#undef BUILD_TFS
#undef BUILD_ASM
#undef BUILD_ASH
    CHECK_EQ(builtin_count, index);
    for (int i = 0; i < builtin_count; i++) {
      Code::cast(builtins_[i])->set_builtin_index(i);
    }
  }

  // Mark as initialized.
  initialized_ = true;
}

void Builtins::TearDown() { initialized_ = false; }

void Builtins::IterateBuiltins(ObjectVisitor* v) {
  v->VisitPointers(&builtins_[0], &builtins_[0] + builtin_count);
}

const char* Builtins::Lookup(byte* pc) {
  // may be called during initialization (disassembler!)
  if (initialized_) {
    for (int i = 0; i < builtin_count; i++) {
      Code* entry = Code::cast(builtins_[i]);
      if (entry->contains(pc)) return name(i);
    }
  }
  return NULL;
}

// static
const char* Builtins::name(int index) {
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return #Name;
    BUILTIN_LIST_ALL(CASE)
#undef CASE
    default:
      UNREACHABLE();
      break;
  }
  return "";
}

// static
Address Builtins::CppEntryOf(int index) {
  DCHECK(0 <= index && index < builtin_count);
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return FUNCTION_ADDR(Builtin_##Name);
    BUILTIN_LIST_C(CASE)
#undef CASE
    default:
      return nullptr;
  }
  UNREACHABLE();
}

// static
bool Builtins::IsCpp(int index) {
  DCHECK(0 <= index && index < builtin_count);
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return true;
#define BUILTIN_LIST_CPP(V)                                       \
  BUILTIN_LIST(V, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)
    BUILTIN_LIST_CPP(CASE)
#undef BUILTIN_LIST_CPP
#undef CASE
    default:
      return false;
  }
  UNREACHABLE();
}

// static
bool Builtins::IsApi(int index) {
  DCHECK(0 <= index && index < builtin_count);
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return true;
#define BUILTIN_LIST_API(V)                                       \
  BUILTIN_LIST(IGNORE_BUILTIN, V, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)
    BUILTIN_LIST_API(CASE);
#undef BUILTIN_LIST_API
#undef CASE
    default:
      return false;
  }
  UNREACHABLE();
}

// static
bool Builtins::HasCppImplementation(int index) {
  DCHECK(0 <= index && index < builtin_count);
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return true;
    BUILTIN_LIST_C(CASE)
#undef CASE
    default:
      return false;
  }
  UNREACHABLE();
}

#define DEFINE_BUILTIN_ACCESSOR(Name, ...)                                    \
  Handle<Code> Builtins::Name() {                                             \
    Code** code_address = reinterpret_cast<Code**>(builtin_address(k##Name)); \
    return Handle<Code>(code_address);                                        \
  }
BUILTIN_LIST_ALL(DEFINE_BUILTIN_ACCESSOR)
#undef DEFINE_BUILTIN_ACCESSOR

// static
bool Builtins::AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                    Handle<JSObject> target_global_proxy) {
  if (FLAG_allow_unsafe_function_constructor) return true;
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  Handle<Context> responsible_context =
      impl->MicrotaskContextIsLastEnteredContext() ? impl->MicrotaskContext()
                                                   : impl->LastEnteredContext();
  // TODO(jochen): Remove this.
  if (responsible_context.is_null()) {
    return true;
  }
  if (*responsible_context == target->context()) return true;
  return isolate->MayAccess(responsible_context, target_global_proxy);
}

}  // namespace internal
}  // namespace v8
