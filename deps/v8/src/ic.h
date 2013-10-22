// Copyright 2012 the V8 project authors. All rights reserved.
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
  ICU(StoreIC_Slow)                                   \
  ICU(SharedStoreIC_ExtendStorage)                    \
  ICU(KeyedStoreIC_Miss)                              \
  ICU(KeyedStoreIC_MissForceGeneric)                  \
  ICU(KeyedStoreIC_Slow)                              \
  /* Utilities for IC stubs. */                       \
  ICU(StoreCallbackProperty)                          \
  ICU(LoadPropertyWithInterceptorOnly)                \
  ICU(LoadPropertyWithInterceptorForLoad)             \
  ICU(LoadPropertyWithInterceptorForCall)             \
  ICU(KeyedLoadPropertyWithInterceptor)               \
  ICU(StoreInterceptorProperty)                       \
  ICU(BinaryOp_Patch)                                 \
  ICU(CompareIC_Miss)                                 \
  ICU(CompareNilIC_Miss)                              \
  ICU(Unreachable)                                    \
  ICU(ToBooleanIC_Miss)
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
  virtual ~IC() {}

  // Get the call-site target; used for determining the state.
  Code* target() const { return GetTargetAtAddress(address()); }
  inline Address address() const;

  // Compute the current IC state based on the target stub, receiver and name.
  static State StateFrom(Code* target, Object* receiver, Object* name);

  // Clear the inline cache to initial state.
  static void Clear(Isolate* isolate, Address address);

  // Computes the reloc info for this IC. This is a fairly expensive
  // operation as it has to search through the heap to find the code
  // object that contains this IC site.
  RelocInfo::Mode ComputeMode();

  // Returns if this IC is for contextual (no explicit receiver)
  // access to properties.
  bool IsUndeclaredGlobal(Handle<Object> receiver) {
    if (receiver->IsGlobalObject()) {
      return SlowIsUndeclaredGlobal();
    } else {
      ASSERT(!SlowIsUndeclaredGlobal());
      return false;
    }
  }

  bool SlowIsUndeclaredGlobal() {
    return ComputeMode() == RelocInfo::CODE_TARGET_CONTEXT;
  }

  // Determines which map must be used for keeping the code stub.
  // These methods should not be called with undefined or null.
  static inline InlineCacheHolderFlag GetCodeCacheForObject(Object* object,
                                                            JSObject* holder);
  static inline InlineCacheHolderFlag GetCodeCacheForObject(JSObject* object,
                                                            JSObject* holder);
  static inline JSObject* GetCodeCacheHolder(Isolate* isolate,
                                             Object* object,
                                             InlineCacheHolderFlag holder);

 protected:
  Address fp() const { return fp_; }
  Address pc() const { return *pc_address_; }
  Isolate* isolate() const { return isolate_; }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Computes the address in the original code when the code running is
  // containing break points (calls to DebugBreakXXX builtins).
  Address OriginalCodeAddress() const;
#endif

  // Set the call-site target.
  void set_target(Code* code) { SetTargetAtAddress(address(), code); }

#ifdef DEBUG
  char TransitionMarkFromState(IC::State state);

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
  static void PostPatching(Address address, Code* target, Code* old_target);

  virtual void UpdateMonomorphicIC(Handle<HeapObject> receiver,
                                   Handle<Code> handler,
                                   Handle<String> name,
                                   StrictModeFlag strict_mode) {
    set_target(*handler);
  }
  bool UpdatePolymorphicIC(State state,
                           Handle<HeapObject> receiver,
                           Handle<String> name,
                           Handle<Code> code,
                           StrictModeFlag strict_mode);

  virtual Handle<Code> ComputePolymorphicIC(MapHandleList* receiver_maps,
                                            CodeHandleList* handlers,
                                            int number_of_valid_maps,
                                            Handle<Name> name,
                                            StrictModeFlag strict_mode) {
    UNREACHABLE();
    return Handle<Code>::null();
  };

  void CopyICToMegamorphicCache(Handle<String> name);
  bool IsTransitionedMapOfMonomorphicTarget(Map* receiver_map);
  void PatchCache(State state,
                  StrictModeFlag strict_mode,
                  Handle<HeapObject> receiver,
                  Handle<String> name,
                  Handle<Code> code);
  virtual void UpdateMegamorphicCache(Map* map, Name* name, Code* code);
  virtual Handle<Code> megamorphic_stub() {
    UNREACHABLE();
    return Handle<Code>::null();
  }
  virtual Handle<Code> megamorphic_stub_strict() {
    UNREACHABLE();
    return Handle<Code>::null();
  }
  virtual Handle<Code> generic_stub() const {
    UNREACHABLE();
    return Handle<Code>::null();
  }
  virtual Handle<Code> generic_stub_strict() const {
    UNREACHABLE();
    return Handle<Code>::null();
  }

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

  // Returns a JSFunction or a Failure.
  MUST_USE_RESULT MaybeObject* LoadFunction(State state,
                                            Code::ExtraICState extra_ic_state,
                                            Handle<Object> object,
                                            Handle<String> name);

 protected:
  CallICBase(Code::Kind kind, Isolate* isolate)
      : IC(EXTRA_CALL_FRAME, isolate), kind_(kind) {}

  bool TryUpdateExtraICState(LookupResult* lookup,
                             Handle<Object> object,
                             Code::ExtraICState* extra_ic_state);

  // Compute a monomorphic stub if possible, otherwise return a null handle.
  Handle<Code> ComputeMonomorphicStub(LookupResult* lookup,
                                      State state,
                                      Code::ExtraICState extra_state,
                                      Handle<Object> object,
                                      Handle<String> name);

  // Update the inline cache and the global stub cache based on the lookup
  // result.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Code::ExtraICState extra_ic_state,
                    Handle<Object> object,
                    Handle<String> name);

  // Returns a JSFunction if the object can be called as a function, and
  // patches the stack to be ready for the call.  Otherwise, it returns the
  // undefined value.
  Handle<Object> TryCallAsFunction(Handle<Object> object);

  void ReceiverToObjectIfRequired(Handle<Object> callee, Handle<Object> object);

  static void Clear(Address address, Code* target);

  // Platform-specific code generation functions used by both call and
  // keyed call.
  static void GenerateMiss(MacroAssembler* masm,
                           int argc,
                           IC::UtilityId id,
                           Code::ExtraICState extra_state);

  static void GenerateNormal(MacroAssembler* masm, int argc);

  static void GenerateMonomorphicCacheProbe(MacroAssembler* masm,
                                            int argc,
                                            Code::Kind kind,
                                            Code::ExtraICState extra_state);

  Code::Kind kind_;

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
                                 Code::ExtraICState extra_state) {
    GenerateMiss(masm, argc, extra_state);
  }

  static void GenerateMiss(MacroAssembler* masm,
                           int argc,
                           Code::ExtraICState extra_state) {
    CallICBase::GenerateMiss(masm, argc, IC::kCallIC_Miss, extra_state);
  }

  static void GenerateMegamorphic(MacroAssembler* masm,
                                  int argc,
                                  Code::ExtraICState extra_ic_state);

  static void GenerateNormal(MacroAssembler* masm, int argc) {
    CallICBase::GenerateNormal(masm, argc);
    GenerateMiss(masm, argc, Code::kNoExtraICState);
  }
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

  static void GenerateMiss(MacroAssembler* masm, int argc) {
    CallICBase::GenerateMiss(masm, argc, IC::kKeyedCallIC_Miss,
                             Code::kNoExtraICState);
  }

  static void GenerateMegamorphic(MacroAssembler* masm, int argc);
  static void GenerateNormal(MacroAssembler* masm, int argc);
  static void GenerateNonStrictArguments(MacroAssembler* masm, int argc);
};


class LoadIC: public IC {
 public:
  explicit LoadIC(FrameDepth depth, Isolate* isolate) : IC(depth, isolate) {
    ASSERT(target()->is_load_stub() || target()->is_keyed_load_stub());
  }

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm);
  static void GenerateNormal(MacroAssembler* masm);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);

  MUST_USE_RESULT MaybeObject* Load(State state,
                                    Handle<Object> object,
                                    Handle<String> name);

 protected:
  virtual Code::Kind kind() const { return Code::LOAD_IC; }

  virtual Handle<Code> slow_stub() const {
    return isolate()->builtins()->LoadIC_Slow();
  }

  virtual Handle<Code> megamorphic_stub() {
    return isolate()->builtins()->LoadIC_Megamorphic();
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Handle<Object> object,
                    Handle<String> name);

  virtual void UpdateMonomorphicIC(Handle<HeapObject> receiver,
                                   Handle<Code> handler,
                                   Handle<String> name,
                                   StrictModeFlag strict_mode);

  virtual Handle<Code> ComputePolymorphicIC(MapHandleList* receiver_maps,
                                            CodeHandleList* handlers,
                                            int number_of_valid_maps,
                                            Handle<Name> name,
                                            StrictModeFlag strict_mode);

  virtual Handle<Code> ComputeLoadHandler(LookupResult* lookup,
                                          Handle<JSObject> receiver,
                                          Handle<String> name);

 private:
  // Stub accessors.
  static Handle<Code> initialize_stub(Isolate* isolate) {
    return isolate->builtins()->LoadIC_Initialize();
  }
  virtual Handle<Code> pre_monomorphic_stub() {
    return isolate()->builtins()->LoadIC_PreMonomorphic();
  }

  static void Clear(Isolate* isolate, Address address, Code* target);

  friend class IC;
};


enum ICMissMode {
  MISS_FORCE_GENERIC,
  MISS
};


class KeyedLoadIC: public LoadIC {
 public:
  explicit KeyedLoadIC(FrameDepth depth, Isolate* isolate)
      : LoadIC(depth, isolate) {
    ASSERT(target()->is_keyed_load_stub());
  }

  MUST_USE_RESULT MaybeObject* Load(State state,
                                    Handle<Object> object,
                                    Handle<Object> key,
                                    ICMissMode force_generic);

  // Code generator routines.
  static void GenerateMiss(MacroAssembler* masm, ICMissMode force_generic);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm) {
    GenerateMiss(masm, MISS);
  }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm, MISS);
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

 protected:
  virtual Code::Kind kind() const { return Code::KEYED_LOAD_IC; }

  Handle<Code> LoadElementStub(Handle<JSObject> receiver);

  virtual Handle<Code> megamorphic_stub() {
    return isolate()->builtins()->KeyedLoadIC_Generic();
  }
  virtual Handle<Code> generic_stub() const {
    return isolate()->builtins()->KeyedLoadIC_Generic();
  }
  virtual Handle<Code> slow_stub() const {
    return isolate()->builtins()->KeyedLoadIC_Slow();
  }

  // Update the inline cache.
  virtual void UpdateMonomorphicIC(Handle<HeapObject> receiver,
                                   Handle<Code> handler,
                                   Handle<String> name,
                                   StrictModeFlag strict_mode);
  virtual Handle<Code> ComputeLoadHandler(LookupResult* lookup,
                                          Handle<JSObject> receiver,
                                          Handle<String> name);
  virtual void UpdateMegamorphicCache(Map* map, Name* name, Code* code) { }

 private:
  // Stub accessors.
  static Handle<Code> initialize_stub(Isolate* isolate) {
    return isolate->builtins()->KeyedLoadIC_Initialize();
  }
  virtual Handle<Code> pre_monomorphic_stub() {
    return isolate()->builtins()->KeyedLoadIC_PreMonomorphic();
  }
  Handle<Code> indexed_interceptor_stub() {
    return isolate()->builtins()->KeyedLoadIC_IndexedInterceptor();
  }
  Handle<Code> non_strict_arguments_stub() {
    return isolate()->builtins()->KeyedLoadIC_NonStrictArguments();
  }
  Handle<Code> string_stub() {
    return isolate()->builtins()->KeyedLoadIC_String();
  }

  static void Clear(Isolate* isolate, Address address, Code* target);

  friend class IC;
};


class StoreIC: public IC {
 public:
  StoreIC(FrameDepth depth, Isolate* isolate) : IC(depth, isolate) {
    ASSERT(target()->is_store_stub() || target()->is_keyed_store_stub());
  }

  // Code generators for stub routines. Only called once at startup.
  static void GenerateSlow(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm,
                                  StrictModeFlag strict_mode);
  static void GenerateNormal(MacroAssembler* masm);
  static void GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         StrictModeFlag strict_mode);

  MUST_USE_RESULT MaybeObject* Store(
      State state,
      StrictModeFlag strict_mode,
      Handle<Object> object,
      Handle<String> name,
      Handle<Object> value,
      JSReceiver::StoreFromKeyed store_mode =
          JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED);

 protected:
  virtual Code::Kind kind() const { return Code::STORE_IC; }
  virtual Handle<Code> megamorphic_stub() {
    return isolate()->builtins()->StoreIC_Megamorphic();
  }
  // Stub accessors.
  virtual Handle<Code> megamorphic_stub_strict() {
    return isolate()->builtins()->StoreIC_Megamorphic_Strict();
  }
  virtual Handle<Code> generic_stub() const {
    return isolate()->builtins()->StoreIC_Generic();
  }
  virtual Handle<Code> generic_stub_strict() const {
    return isolate()->builtins()->StoreIC_Generic_Strict();
  }
  virtual Handle<Code> pre_monomorphic_stub() const {
    return isolate()->builtins()->StoreIC_PreMonomorphic();
  }
  virtual Handle<Code> pre_monomorphic_stub_strict() const {
    return isolate()->builtins()->StoreIC_PreMonomorphic_Strict();
  }
  virtual Handle<Code> global_proxy_stub() {
    return isolate()->builtins()->StoreIC_GlobalProxy();
  }
  virtual Handle<Code> global_proxy_stub_strict() {
    return isolate()->builtins()->StoreIC_GlobalProxy_Strict();
  }

  virtual void UpdateMonomorphicIC(Handle<HeapObject> receiver,
                                   Handle<Code> handler,
                                   Handle<String> name,
                                   StrictModeFlag strict_mode);

  virtual Handle<Code> ComputePolymorphicIC(MapHandleList* receiver_maps,
                                            CodeHandleList* handlers,
                                            int number_of_valid_maps,
                                            Handle<Name> name,
                                            StrictModeFlag strict_mode);

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    StrictModeFlag strict_mode,
                    Handle<JSObject> receiver,
                    Handle<String> name,
                    Handle<Object> value);
  // Compute the code stub for this store; used for rewriting to
  // monomorphic state and making sure that the code stub is in the
  // stub cache.
  virtual Handle<Code> ComputeStoreMonomorphic(LookupResult* lookup,
                                               StrictModeFlag strict_mode,
                                               Handle<JSObject> receiver,
                                               Handle<String> name,
                                               Handle<Object> value);

 private:
  void set_target(Code* code) {
    // Strict mode must be preserved across IC patching.
    ASSERT(Code::GetStrictMode(code->extra_ic_state()) ==
           Code::GetStrictMode(target()->extra_ic_state()));
    IC::set_target(code);
  }

  static Handle<Code> initialize_stub(Isolate* isolate) {
    return isolate->builtins()->StoreIC_Initialize();
  }
  static Handle<Code> initialize_stub_strict(Isolate* isolate) {
    return isolate->builtins()->StoreIC_Initialize_Strict();
  }
  static void Clear(Isolate* isolate, Address address, Code* target);

  friend class IC;
};


enum KeyedStoreCheckMap {
  kDontCheckMap,
  kCheckMap
};


enum KeyedStoreIncrementLength {
  kDontIncrementLength,
  kIncrementLength
};


class KeyedStoreIC: public StoreIC {
 public:
  KeyedStoreIC(FrameDepth depth, Isolate* isolate)
      : StoreIC(depth, isolate) {
    ASSERT(target()->is_keyed_store_stub());
  }

  MUST_USE_RESULT MaybeObject* Store(State state,
                                     StrictModeFlag strict_mode,
                                     Handle<Object> object,
                                     Handle<Object> name,
                                     Handle<Object> value,
                                     ICMissMode force_generic);

  // Code generators for stub routines.  Only called once at startup.
  static void GenerateInitialize(MacroAssembler* masm) {
    GenerateMiss(masm, MISS);
  }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm, MISS);
  }
  static void GenerateMiss(MacroAssembler* masm, ICMissMode force_generic);
  static void GenerateSlow(MacroAssembler* masm);
  static void GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         StrictModeFlag strict_mode);
  static void GenerateGeneric(MacroAssembler* masm, StrictModeFlag strict_mode);
  static void GenerateNonStrictArguments(MacroAssembler* masm);

 protected:
  virtual Code::Kind kind() const { return Code::KEYED_STORE_IC; }

  virtual Handle<Code> ComputeStoreMonomorphic(LookupResult* lookup,
                                               StrictModeFlag strict_mode,
                                               Handle<JSObject> receiver,
                                               Handle<String> name,
                                               Handle<Object> value);
  virtual void UpdateMegamorphicCache(Map* map, Name* name, Code* code) { }

  virtual Handle<Code> pre_monomorphic_stub() const {
    return isolate()->builtins()->KeyedStoreIC_PreMonomorphic();
  }
  virtual Handle<Code> pre_monomorphic_stub_strict() const {
    return isolate()->builtins()->KeyedStoreIC_PreMonomorphic_Strict();
  }
  virtual Handle<Code> megamorphic_stub() {
    return isolate()->builtins()->KeyedStoreIC_Generic();
  }
  virtual Handle<Code> megamorphic_stub_strict() {
    return isolate()->builtins()->KeyedStoreIC_Generic_Strict();
  }

  Handle<Code> StoreElementStub(Handle<JSObject> receiver,
                                KeyedAccessStoreMode store_mode,
                                StrictModeFlag strict_mode);

  virtual void UpdateMonomorphicIC(Handle<HeapObject> receiver,
                                   Handle<Code> handler,
                                   Handle<String> name,
                                   StrictModeFlag strict_mode);

 private:
  void set_target(Code* code) {
    // Strict mode must be preserved across IC patching.
    ASSERT(Code::GetStrictMode(code->extra_ic_state()) ==
           Code::GetStrictMode(target()->extra_ic_state()));
    IC::set_target(code);
  }

  // Stub accessors.
  static Handle<Code> initialize_stub(Isolate* isolate) {
    return isolate->builtins()->KeyedStoreIC_Initialize();
  }
  static Handle<Code> initialize_stub_strict(Isolate* isolate) {
    return isolate->builtins()->KeyedStoreIC_Initialize_Strict();
  }
  Handle<Code> generic_stub() const {
    return isolate()->builtins()->KeyedStoreIC_Generic();
  }
  Handle<Code> generic_stub_strict() const {
    return isolate()->builtins()->KeyedStoreIC_Generic_Strict();
  }
  Handle<Code> non_strict_arguments_stub() {
    return isolate()->builtins()->KeyedStoreIC_NonStrictArguments();
  }

  static void Clear(Isolate* isolate, Address address, Code* target);

  KeyedAccessStoreMode GetStoreMode(Handle<JSObject> receiver,
                                    Handle<Object> key,
                                    Handle<Object> value);

  Handle<Map> ComputeTransitionedMap(Handle<JSObject> receiver,
                                     KeyedAccessStoreMode store_mode);

  friend class IC;
};


// Type Recording BinaryOpIC, that records the types of the inputs and outputs.
class BinaryOpIC: public IC {
 public:
  enum TypeInfo {
    UNINITIALIZED,
    SMI,
    INT32,
    NUMBER,
    ODDBALL,
    STRING,  // Only used for addition operation.
    GENERIC
  };

  static void StubInfoToType(int minor_key,
                             Handle<Type>* left,
                             Handle<Type>* right,
                             Handle<Type>* result,
                             Isolate* isolate);

  explicit BinaryOpIC(Isolate* isolate) : IC(NO_EXTRA_FRAME, isolate) { }

  void patch(Code* code);

  static const char* GetName(TypeInfo type_info);

  static State ToState(TypeInfo type_info);

 private:
  static Handle<Type> TypeInfoToType(TypeInfo binary_type, Isolate* isolate);
};


class CompareIC: public IC {
 public:
  // The type/state lattice is defined by the following inequations:
  //   UNINITIALIZED < ...
  //   ... < GENERIC
  //   SMI < NUMBER
  //   INTERNALIZED_STRING < STRING
  //   KNOWN_OBJECT < OBJECT
  enum State {
    UNINITIALIZED,
    SMI,
    NUMBER,
    STRING,
    INTERNALIZED_STRING,
    UNIQUE_NAME,    // Symbol or InternalizedString
    OBJECT,         // JSObject
    KNOWN_OBJECT,   // JSObject with specific map (faster check)
    GENERIC
  };

  static State NewInputState(State old_state, Handle<Object> value);

  static Handle<Type> StateToType(Isolate* isolate,
                                  State state,
                                  Handle<Map> map = Handle<Map>());

  static void StubInfoToType(int stub_minor_key,
                             Handle<Type>* left_type,
                             Handle<Type>* right_type,
                             Handle<Type>* overall_type,
                             Handle<Map> map,
                             Isolate* isolate);

  CompareIC(Isolate* isolate, Token::Value op)
      : IC(EXTRA_CALL_FRAME, isolate), op_(op) { }

  // Update the inline cache for the given operands.
  void UpdateCaches(Handle<Object> x, Handle<Object> y);


  // Factory method for getting an uninitialized compare stub.
  static Handle<Code> GetUninitialized(Isolate* isolate, Token::Value op);

  // Helper function for computing the condition for a compare operation.
  static Condition ComputeCondition(Token::Value op);

  static const char* GetStateName(State state);

 private:
  static bool HasInlinedSmiCode(Address address);

  State TargetState(State old_state,
                    State old_left,
                    State old_right,
                    bool has_inlined_smi_code,
                    Handle<Object> x,
                    Handle<Object> y);

  bool strict() const { return op_ == Token::EQ_STRICT; }
  Condition GetCondition() const { return ComputeCondition(op_); }

  static Code* GetRawUninitialized(Isolate* isolate, Token::Value op);

  static void Clear(Isolate* isolate, Address address, Code* target);

  Token::Value op_;

  friend class IC;
};


class CompareNilIC: public IC {
 public:
  explicit CompareNilIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) {}

  MUST_USE_RESULT MaybeObject* CompareNil(Handle<Object> object);

  static Handle<Code> GetUninitialized();

  static void Clear(Address address, Code* target);

  static MUST_USE_RESULT MaybeObject* DoCompareNilSlow(NilValue nil,
                                                       Handle<Object> object);
};


class ToBooleanIC: public IC {
 public:
  explicit ToBooleanIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) { }

  MaybeObject* ToBoolean(Handle<Object> object, Code::ExtraICState state);
};


// Helper for BinaryOpIC and CompareIC.
enum InlinedSmiCheck { ENABLE_INLINED_SMI_CHECK, DISABLE_INLINED_SMI_CHECK };
void PatchInlinedSmiCode(Address address, InlinedSmiCheck check);

DECLARE_RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, UnaryOpIC_Miss);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, StoreIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, ElementsTransitionAndStoreIC_Miss);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, CompareNilIC_Miss);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, ToBooleanIC_Miss);


} }  // namespace v8::internal

#endif  // V8_IC_H_
