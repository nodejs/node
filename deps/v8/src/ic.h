// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_IC_H_
#define V8_IC_H_

#include "macro-assembler.h"
#include "type-info.h"

namespace v8 {
namespace internal {


// IC_UTIL_LIST defines all utility functions called from generated
// inline caching code. The argument for the macro, ICU, is the function name.
#define IC_UTIL_LIST(ICU)                             \
  ICU(LoadIC_Miss)                                    \
  ICU(KeyedLoadIC_Miss)                               \
  ICU(KeyedLoadIC_MissForceGeneric)                   \
  ICU(CallIC_Miss)                                    \
  ICU(KeyedCallIC_Miss)                               \
  ICU(StoreIC_Miss)                                   \
  ICU(StoreIC_ArrayLength)                            \
  ICU(SharedStoreIC_ExtendStorage)                    \
  ICU(KeyedStoreIC_Miss)                              \
  ICU(KeyedStoreIC_MissForceGeneric)                  \
  ICU(KeyedStoreIC_Slow)                              \
  /* Utilities for IC stubs. */                       \
  ICU(LoadCallbackProperty)                           \
  ICU(StoreCallbackProperty)                          \
  ICU(LoadPropertyWithInterceptorOnly)                \
  ICU(LoadPropertyWithInterceptorForLoad)             \
  ICU(LoadPropertyWithInterceptorForCall)             \
  ICU(KeyedLoadPropertyWithInterceptor)               \
  ICU(StoreInterceptorProperty)                       \
  ICU(UnaryOp_Patch)                                  \
  ICU(BinaryOp_Patch)                                 \
  ICU(CompareIC_Miss)                                 \
  ICU(ToBoolean_Patch)
//
// IC is the base class for LoadIC, StoreIC, CallIC, KeyedLoadIC,
// and KeyedStoreIC.
//
class IC {
 public:
  // The ids for utility called from the generated code.
  enum UtilityId {
  #define CONST_NAME(name) k##name,
    IC_UTIL_LIST(CONST_NAME)
  #undef CONST_NAME
    kUtilityCount
  };

  // Looks up the address of the named utility.
  static Address AddressFromUtilityId(UtilityId id);

  // Alias the inline cache state type to make the IC code more readable.
  typedef InlineCacheState State;

  // The IC code is either invoked with no extra frames on the stack
  // or with a single extra frame for supporting calls.
  enum FrameDepth {
    NO_EXTRA_FRAME = 0,
    EXTRA_CALL_FRAME = 1
  };

  // Construct the IC structure with the given number of extra
  // JavaScript frames on the stack.
  IC(FrameDepth depth, Isolate* isolate);

  // Get the call-site target; used for determining the state.
  Code* target() { return GetTargetAtAddress(address()); }
  inline Address address();

  // Compute the current IC state based on the target stub, receiver and name.
  static State StateFrom(Code* target, Object* receiver, Object* name);

  // Clear the inline cache to initial state.
  static void Clear(Address address);

  // Computes the reloc info for this IC. This is a fairly expensive
  // operation as it has to search through the heap to find the code
  // object that contains this IC site.
  RelocInfo::Mode ComputeMode();

  // Returns if this IC is for contextual (no explicit receiver)
  // access to properties.
  bool IsContextual(Handle<Object> receiver) {
    if (receiver->IsGlobalObject()) {
      return SlowIsContextual();
    } else {
      ASSERT(!SlowIsContextual());
      return false;
    }
  }

  bool SlowIsContextual() {
    return ComputeMode() == RelocInfo::CODE_TARGET_CONTEXT;
  }

  // Determines which map must be used for keeping the code stub.
  // These methods should not be called with undefined or null.
  static inline InlineCacheHolderFlag GetCodeCacheForObject(Object* object,
                                                            JSObject* holder);
  static inline InlineCacheHolderFlag GetCodeCacheForObject(JSObject* object,
                                                            JSObject* holder);
  static inline JSObject* GetCodeCacheHolder(Object* object,
                                             InlineCacheHolderFlag holder);

 protected:
  Address fp() const { return fp_; }
  Address pc() const { return *pc_address_; }
  Isolate* isolate() const { return isolate_; }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Computes the address in the original code when the code running is
  // containing break points (calls to DebugBreakXXX builtins).
  Address OriginalCodeAddress();
#endif

  // Set the call-site target.
  void set_target(Code* code) { SetTargetAtAddress(address(), code); }

#ifdef DEBUG
  void TraceIC(const char* type,
               Handle<Object> name,
               State old_state,
               Code* new_target);
#endif

  Failure* TypeError(const char* type,
                     Handle<Object> object,
                     Handle<Object> key);
  Failure* ReferenceError(const char* type, Handle<String> name);

  // Access the target code for the given IC address.
  static inline Code* GetTargetAtAddress(Address address);
  static inline void SetTargetAtAddress(Address address, Code* target);

 private:
  // Frame pointer for the frame that uses (calls) the IC.
  Address fp_;

  // All access to the program counter of an IC structure is indirect
  // to make the code GC safe. This feature is crucial since
  // GetProperty and SetProperty are called and they in turn might
  // invoke the garbage collector.
  Address* pc_address_;

  Isolate* isolate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IC);
};


// An IC_Utility encapsulates IC::UtilityId. It exists mainly because you
// cannot make forward declarations to an enum.
class IC_Utility {
 public:
  explicit IC_Utility(IC::UtilityId id)
    : address_(IC::AddressFromUtilityId(id)), id_(id) {}

  Address address() const { return address_; }

  IC::UtilityId id() const { return id_; }
 private:
  Address address_;
  IC::UtilityId id_;
};


class CallICBase: public IC {
 public:
  class Contextual: public BitField<bool, 0, 1> {};
  class StringStubState: public BitField<StringStubFeedback, 1, 1> {};

 protected:
  CallICBase(Code::Kind kind, Isolate* isolate)
      : IC(EXTRA_CALL_FRAME, isolate), kind_(kind) {}

 public:
  MUST_USE_RESULT MaybeObject* LoadFunction(State state,
                                            Code::ExtraICState extra_ic_state,
                                            Handle<Object> object,
                                            Handle<String> name);

 protected:
  Code::Kind kind_;

  bool TryUpdateExtraICState(LookupResult* lookup,
                             Handle<Object> object,
                             Code::ExtraICState* extra_ic_state);

  MUST_USE_RESULT MaybeObject* ComputeMonomorphicStub(
      LookupResult* lookup,
      State state,
      Code::ExtraICState extra_ic_state,
      Handle<Object> object,
      Handle<String> name);

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Code::ExtraICState extra_ic_state,
                    Handle<Object> object,
                    Handle<String> name);

  // Returns a JSFunction if the object can be called as a function,
  // and patches the stack to be ready for the call.
  // Otherwise, it returns the undefined value.
  Object* TryCallAsFunction(Object* object);

  void ReceiverToObjectIfRequired(Handle<Object> callee, Handle<Object> object);

  static void Clear(Address address, Code* target);

  friend class IC;
};


class CallIC: public CallICBase {
 public:
  explicit CallIC(Isolate* isolate) : CallICBase(Code::CALL_IC, isolate) {
    ASSERT(target()->is_call_stub());
  }

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm,
                                 int argc,
                                 Code::ExtraICState extra_ic_state) {
    GenerateMiss(masm, argc, extra_ic_state);
  }
  static void GenerateMiss(MacroAssembler* masm,
                           int argc,
                           Code::ExtraICState extra_ic_state);
  static void GenerateMegamorphic(MacroAssembler* masm,
                                  int argc,
                                  Code::ExtraICState extra_ic_state);
  static void GenerateNormal(MacroAssembler* masm, int argc);
};


class KeyedCallIC: public CallICBase {
 public:
  explicit KeyedCallIC(Isolate* isolate)
      : CallICBase(Code::KEYED_CALL_IC, isolate) {
    ASSERT(target()->is_keyed_call_stub());
  }

  MUST_USE_RESULT MaybeObject* LoadFunction(State state,
                                            Handle<Object> object,
                                            Handle<Object> key);

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm, int argc) {
    GenerateMiss(masm, argc);
  }
  static void GenerateMiss(MacroAssembler* masm, int argc);
  static void GenerateMegamorphic(MacroAssembler* masm, int argc);
  static void GenerateNormal(MacroAssembler* masm, int argc);
  static void GenerateNonStrictArguments(MacroAssembler* masm, int argc);
};


class LoadIC: public IC {
 public:
  explicit LoadIC(Isolate* isolate) : IC(NO_EXTRA_FRAME, isolate) {
    ASSERT(target()->is_load_stub());
  }

  MUST_USE_RESULT MaybeObject* Load(State state,
                                    Handle<Object> object,
                                    Handle<String> name);

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm);
  static void GenerateNormal(MacroAssembler* masm);

  // Specialized code generator routines.
  static void GenerateArrayLength(MacroAssembler* masm);
  static void GenerateStringLength(MacroAssembler* masm,
                                   bool support_wrappers);
  static void GenerateFunctionPrototype(MacroAssembler* masm);

 private:
  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Handle<Object> object,
                    Handle<String> name);

  // Stub accessors.
  Code* megamorphic_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kLoadIC_Megamorphic);
  }
  static Code* initialize_stub() {
    return Isolate::Current()->builtins()->builtin(
        Builtins::kLoadIC_Initialize);
  }
  Code* pre_monomorphic_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kLoadIC_PreMonomorphic);
  }

  static void Clear(Address address, Code* target);

  friend class IC;
};


class KeyedIC: public IC {
 public:
  explicit KeyedIC(Isolate* isolate) : IC(NO_EXTRA_FRAME, isolate) {}
  virtual ~KeyedIC() {}

  virtual MaybeObject* GetElementStubWithoutMapCheck(
      bool is_js_array,
      ElementsKind elements_kind) = 0;

 protected:
  virtual Code* string_stub() {
    return NULL;
  }

  virtual Code::Kind kind() const = 0;

  MaybeObject* ComputeStub(JSObject* receiver,
                           bool is_store,
                           StrictModeFlag strict_mode,
                           Code* default_stub);

  virtual MaybeObject* ConstructMegamorphicStub(
      MapList* receiver_maps,
      CodeList* targets,
      StrictModeFlag strict_mode) = 0;

 private:
  void GetReceiverMapsForStub(Code* stub, MapList* result);

  MaybeObject* ComputeMonomorphicStubWithoutMapCheck(
      Map* receiver_map,
      StrictModeFlag strict_mode);

  MaybeObject* ComputeMonomorphicStub(JSObject* receiver,
                                      bool is_store,
                                      StrictModeFlag strict_mode,
                                      Code* default_stub);
};


class KeyedLoadIC: public KeyedIC {
 public:
  explicit KeyedLoadIC(Isolate* isolate) : KeyedIC(isolate) {
    ASSERT(target()->is_keyed_load_stub());
  }

  MUST_USE_RESULT MaybeObject* Load(State state,
                                    Handle<Object> object,
                                    Handle<Object> key,
                                    bool force_generic_stub);

  // Code generator routines.
  static void GenerateMiss(MacroAssembler* masm, bool force_generic);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm) {
    GenerateMiss(masm, false);
  }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm, false);
  }
  static void GenerateGeneric(MacroAssembler* masm);
  static void GenerateString(MacroAssembler* masm);
  static void GenerateIndexedInterceptor(MacroAssembler* masm);
  static void GenerateNonStrictArguments(MacroAssembler* masm);

  // Bit mask to be tested against bit field for the cases when
  // generic stub should go into slow case.
  // Access check is necessary explicitly since generic stub does not perform
  // map checks.
  static const int kSlowCaseBitFieldMask =
      (1 << Map::kIsAccessCheckNeeded) | (1 << Map::kHasIndexedInterceptor);

  virtual MaybeObject* GetElementStubWithoutMapCheck(
      bool is_js_array,
      ElementsKind elements_kind);

 protected:
  virtual Code::Kind kind() const { return Code::KEYED_LOAD_IC; }

  virtual MaybeObject* ConstructMegamorphicStub(
      MapList* receiver_maps,
      CodeList* targets,
      StrictModeFlag strict_mode);

  virtual Code* string_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedLoadIC_String);
  }

 private:
  // Update the inline cache.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Handle<Object> object,
                    Handle<String> name);

  // Stub accessors.
  static Code* initialize_stub() {
    return Isolate::Current()->builtins()->builtin(
        Builtins::kKeyedLoadIC_Initialize);
  }
  Code* megamorphic_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedLoadIC_Generic);
  }
  Code* generic_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedLoadIC_Generic);
  }
  Code* pre_monomorphic_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedLoadIC_PreMonomorphic);
  }
  Code* indexed_interceptor_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedLoadIC_IndexedInterceptor);
  }
  Code* non_strict_arguments_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedLoadIC_NonStrictArguments);
  }

  static void Clear(Address address, Code* target);

  friend class IC;
};


class StoreIC: public IC {
 public:
  explicit StoreIC(Isolate* isolate) : IC(NO_EXTRA_FRAME, isolate) {
    ASSERT(target()->is_store_stub());
  }

  MUST_USE_RESULT MaybeObject* Store(State state,
                                     StrictModeFlag strict_mode,
                                     Handle<Object> object,
                                     Handle<String> name,
                                     Handle<Object> value);

  // Code generators for stub routines. Only called once at startup.
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm,
                                  StrictModeFlag strict_mode);
  static void GenerateArrayLength(MacroAssembler* masm);
  static void GenerateNormal(MacroAssembler* masm);
  static void GenerateGlobalProxy(MacroAssembler* masm,
                                  StrictModeFlag strict_mode);

 private:
  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    StrictModeFlag strict_mode,
                    Handle<JSObject> receiver,
                    Handle<String> name,
                    Handle<Object> value);

  void set_target(Code* code) {
    // Strict mode must be preserved across IC patching.
    ASSERT((code->extra_ic_state() & kStrictMode) ==
           (target()->extra_ic_state() & kStrictMode));
    IC::set_target(code);
  }

  // Stub accessors.
  Code* megamorphic_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kStoreIC_Megamorphic);
  }
  Code* megamorphic_stub_strict() {
    return isolate()->builtins()->builtin(
        Builtins::kStoreIC_Megamorphic_Strict);
  }
  static Code* initialize_stub() {
    return Isolate::Current()->builtins()->builtin(
        Builtins::kStoreIC_Initialize);
  }
  static Code* initialize_stub_strict() {
    return Isolate::Current()->builtins()->builtin(
        Builtins::kStoreIC_Initialize_Strict);
  }
  Code* global_proxy_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kStoreIC_GlobalProxy);
  }
  Code* global_proxy_stub_strict() {
    return isolate()->builtins()->builtin(
        Builtins::kStoreIC_GlobalProxy_Strict);
  }

  static void Clear(Address address, Code* target);

  friend class IC;
};


class KeyedStoreIC: public KeyedIC {
 public:
  explicit KeyedStoreIC(Isolate* isolate) : KeyedIC(isolate) {
    ASSERT(target()->is_keyed_store_stub());
  }

  MUST_USE_RESULT MaybeObject* Store(State state,
                                   StrictModeFlag strict_mode,
                                     Handle<Object> object,
                                     Handle<Object> name,
                                     Handle<Object> value,
                                     bool force_generic);

  // Code generators for stub routines.  Only called once at startup.
  static void GenerateInitialize(MacroAssembler* masm) {
    GenerateMiss(masm, false);
  }
  static void GenerateMiss(MacroAssembler* masm, bool force_generic);
  static void GenerateSlow(MacroAssembler* masm);
  static void GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         StrictModeFlag strict_mode);
  static void GenerateGeneric(MacroAssembler* masm, StrictModeFlag strict_mode);
  static void GenerateNonStrictArguments(MacroAssembler* masm);

  virtual MaybeObject* GetElementStubWithoutMapCheck(
      bool is_js_array,
      ElementsKind elements_kind);

 protected:
  virtual Code::Kind kind() const { return Code::KEYED_STORE_IC; }

  virtual MaybeObject* ConstructMegamorphicStub(
      MapList* receiver_maps,
      CodeList* targets,
      StrictModeFlag strict_mode);

  private:
  // Update the inline cache.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    StrictModeFlag strict_mode,
                    Handle<JSObject> receiver,
                    Handle<String> name,
                    Handle<Object> value);

  void set_target(Code* code) {
    // Strict mode must be preserved across IC patching.
    ASSERT((code->extra_ic_state() & kStrictMode) ==
           (target()->extra_ic_state() & kStrictMode));
    IC::set_target(code);
  }

  // Stub accessors.
  static Code* initialize_stub() {
    return Isolate::Current()->builtins()->builtin(
        Builtins::kKeyedStoreIC_Initialize);
  }
  Code* megamorphic_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedStoreIC_Generic);
  }
  static Code* initialize_stub_strict() {
    return Isolate::Current()->builtins()->builtin(
        Builtins::kKeyedStoreIC_Initialize_Strict);
  }
  Code* megamorphic_stub_strict() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedStoreIC_Generic_Strict);
  }
  Code* generic_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedStoreIC_Generic);
  }
  Code* generic_stub_strict() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedStoreIC_Generic_Strict);
  }
  Code* non_strict_arguments_stub() {
    return isolate()->builtins()->builtin(
        Builtins::kKeyedStoreIC_NonStrictArguments);
  }

  static void Clear(Address address, Code* target);

  friend class IC;
};


class UnaryOpIC: public IC {
 public:
  // sorted: increasingly more unspecific (ignoring UNINITIALIZED)
  // TODO(svenpanne) Using enums+switch is an antipattern, use a class instead.
  enum TypeInfo {
    UNINITIALIZED,
    SMI,
    HEAP_NUMBER,
    GENERIC
  };

  explicit UnaryOpIC(Isolate* isolate) : IC(NO_EXTRA_FRAME, isolate) { }

  void patch(Code* code);

  static const char* GetName(TypeInfo type_info);

  static State ToState(TypeInfo type_info);

  static TypeInfo GetTypeInfo(Handle<Object> operand);

  static TypeInfo ComputeNewType(TypeInfo type, TypeInfo previous);
};


// Type Recording BinaryOpIC, that records the types of the inputs and outputs.
class BinaryOpIC: public IC {
 public:
  enum TypeInfo {
    UNINITIALIZED,
    SMI,
    INT32,
    HEAP_NUMBER,
    ODDBALL,
    BOTH_STRING,  // Only used for addition operation.
    STRING,  // Only used for addition operation.  At least one string operand.
    GENERIC
  };

  explicit BinaryOpIC(Isolate* isolate) : IC(NO_EXTRA_FRAME, isolate) { }

  void patch(Code* code);

  static const char* GetName(TypeInfo type_info);

  static State ToState(TypeInfo type_info);

  static TypeInfo GetTypeInfo(Handle<Object> left, Handle<Object> right);

  static TypeInfo JoinTypes(TypeInfo x, TypeInfo y);
};


class CompareIC: public IC {
 public:
  enum State {
    UNINITIALIZED,
    SMIS,
    HEAP_NUMBERS,
    SYMBOLS,
    STRINGS,
    OBJECTS,
    GENERIC
  };

  CompareIC(Isolate* isolate, Token::Value op)
      : IC(EXTRA_CALL_FRAME, isolate), op_(op) { }

  // Update the inline cache for the given operands.
  void UpdateCaches(Handle<Object> x, Handle<Object> y);

  // Factory method for getting an uninitialized compare stub.
  static Handle<Code> GetUninitialized(Token::Value op);

  // Helper function for computing the condition for a compare operation.
  static Condition ComputeCondition(Token::Value op);

  // Helper function for determining the state of a compare IC.
  static State ComputeState(Code* target);

  static const char* GetStateName(State state);

 private:
  State TargetState(State state, bool has_inlined_smi_code,
                    Handle<Object> x, Handle<Object> y);

  bool strict() const { return op_ == Token::EQ_STRICT; }
  Condition GetCondition() const { return ComputeCondition(op_); }
  State GetState() { return ComputeState(target()); }

  Token::Value op_;
};


class ToBooleanIC: public IC {
 public:
  explicit ToBooleanIC(Isolate* isolate) : IC(NO_EXTRA_FRAME, isolate) { }

  void patch(Code* code);
};


// Helper for BinaryOpIC and CompareIC.
void PatchInlinedSmiCode(Address address);

} }  // namespace v8::internal

#endif  // V8_IC_H_
