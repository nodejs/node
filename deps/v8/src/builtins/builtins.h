// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_H_
#define V8_BUILTINS_BUILTINS_H_

#include "src/base/flags.h"
#include "src/builtins/builtins-definitions.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Callable;
template <typename T>
class Handle;
class Isolate;

// Forward declarations.
class RootVisitor;
enum class InterpreterPushArgsMode : unsigned;
namespace compiler {
class CodeAssemblerState;
}

class Builtins {
 public:
  ~Builtins();

  void TearDown();

  // Garbage collection support.
  void IterateBuiltins(RootVisitor* v);

  // Disassembler support.
  const char* Lookup(byte* pc);

  enum Name : int32_t {
#define DEF_ENUM(Name, ...) k##Name,
    BUILTIN_LIST_ALL(DEF_ENUM)
#undef DEF_ENUM
        builtin_count
  };

#define DECLARE_BUILTIN_ACCESSOR(Name, ...) \
  V8_EXPORT_PRIVATE Handle<Code> Name();
  BUILTIN_LIST_ALL(DECLARE_BUILTIN_ACCESSOR)
#undef DECLARE_BUILTIN_ACCESSOR

  // Convenience wrappers.
  Handle<Code> CallFunction(
      ConvertReceiverMode = ConvertReceiverMode::kAny,
      TailCallMode tail_call_mode = TailCallMode::kDisallow);
  Handle<Code> Call(ConvertReceiverMode = ConvertReceiverMode::kAny,
                    TailCallMode tail_call_mode = TailCallMode::kDisallow);
  Handle<Code> CallBoundFunction(TailCallMode tail_call_mode);
  Handle<Code> NonPrimitiveToPrimitive(
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);
  Handle<Code> OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint);
  Handle<Code> InterpreterPushArgsThenCall(ConvertReceiverMode receiver_mode,
                                           TailCallMode tail_call_mode,
                                           InterpreterPushArgsMode mode);
  Handle<Code> InterpreterPushArgsThenConstruct(InterpreterPushArgsMode mode);
  Handle<Code> NewFunctionContext(ScopeType scope_type);
  Handle<Code> NewCloneShallowArray(AllocationSiteMode allocation_mode);
  Handle<Code> JSConstructStubGeneric();

  Code* builtin(Name name) {
    // Code::cast cannot be used here since we access builtins
    // during the marking phase of mark sweep. See IC::Clear.
    return reinterpret_cast<Code*>(builtins_[name]);
  }

  Address builtin_address(Name name) {
    return reinterpret_cast<Address>(&builtins_[name]);
  }

  static int GetBuiltinParameterCount(Name name);

  static Callable CallableFor(Isolate* isolate, Name name);

  static const char* name(int index);

  // Returns the C++ entry point for builtins implemented in C++, and the null
  // Address otherwise.
  static Address CppEntryOf(int index);

  static bool IsCpp(int index);
  static bool IsApi(int index);
  static bool HasCppImplementation(int index);

  bool is_initialized() const { return initialized_; }

  // Used by SetupIsolateDelegate and Deserializer.
  void MarkInitialized() {
    DCHECK(!initialized_);
    initialized_ = true;
  }

  MUST_USE_RESULT static MaybeHandle<Object> InvokeApiFunction(
      Isolate* isolate, bool is_construct, Handle<HeapObject> function,
      Handle<Object> receiver, int argc, Handle<Object> args[],
      Handle<HeapObject> new_target);

  enum ExitFrameType { EXIT, BUILTIN_EXIT };

  static void Generate_Adaptor(MacroAssembler* masm, Address builtin_address,
                               ExitFrameType exit_frame_type);

  static bool AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                   Handle<JSObject> target_global_proxy);

 private:
  Builtins();

  static void Generate_CallFunction(MacroAssembler* masm,
                                    ConvertReceiverMode mode,
                                    TailCallMode tail_call_mode);

  static void Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                             TailCallMode tail_call_mode);

  static void Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                            TailCallMode tail_call_mode);

  static void Generate_ForwardVarargs(MacroAssembler* masm, Handle<Code> code);

  static void Generate_InterpreterPushArgsThenCallImpl(
      MacroAssembler* masm, ConvertReceiverMode receiver_mode,
      TailCallMode tail_call_mode, InterpreterPushArgsMode mode);

  static void Generate_InterpreterPushArgsThenConstructImpl(
      MacroAssembler* masm, InterpreterPushArgsMode mode);

#define DECLARE_ASM(Name, ...) \
  static void Generate_##Name(MacroAssembler* masm);
#define DECLARE_TF(Name, ...) \
  static void Generate_##Name(compiler::CodeAssemblerState* state);

  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, DECLARE_TF, DECLARE_TF,
               DECLARE_TF, DECLARE_TF, DECLARE_ASM, DECLARE_ASM)

#undef DECLARE_ASM
#undef DECLARE_TF

  // Note: These are always Code objects, but to conform with
  // IterateBuiltins() above which assumes Object**'s for the callback
  // function f, we use an Object* array here.
  Object* builtins_[builtin_count];
  bool initialized_;

  friend class Isolate;
  friend class SetupIsolateDelegate;

  DISALLOW_COPY_AND_ASSIGN(Builtins);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_H_
