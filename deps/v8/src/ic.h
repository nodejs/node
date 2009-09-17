// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#include "assembler.h"

namespace v8 {
namespace internal {

// IC_UTIL_LIST defines all utility functions called from generated
// inline caching code. The argument for the macro, ICU, is the function name.
#define IC_UTIL_LIST(ICU)                             \
  ICU(LoadIC_Miss)                                    \
  ICU(KeyedLoadIC_Miss)                               \
  ICU(CallIC_Miss)                                    \
  ICU(StoreIC_Miss)                                   \
  ICU(SharedStoreIC_ExtendStorage)                    \
  ICU(KeyedStoreIC_Miss)                              \
  /* Utilities for IC stubs. */                       \
  ICU(LoadCallbackProperty)                           \
  ICU(StoreCallbackProperty)                          \
  ICU(LoadPropertyWithInterceptorOnly)                \
  ICU(LoadPropertyWithInterceptorForLoad)             \
  ICU(LoadPropertyWithInterceptorForCall)             \
  ICU(StoreInterceptorProperty)

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
  explicit IC(FrameDepth depth);

  // Get the call-site target; used for determining the state.
  Code* target() { return GetTargetAtAddress(address()); }
  inline Address address();

  // Compute the current IC state based on the target stub and the receiver.
  static State StateFrom(Code* target, Object* receiver);

  // Clear the inline cache to initial state.
  static void Clear(Address address);

  // Computes the reloc info for this IC. This is a fairly expensive
  // operation as it has to search through the heap to find the code
  // object that contains this IC site.
  RelocInfo::Mode ComputeMode();

  // Returns if this IC is for contextual (no explicit receiver)
  // access to properties.
  bool is_contextual() {
    return ComputeMode() == RelocInfo::CODE_TARGET_CONTEXT;
  }

  // Returns the map to use for caching stubs for a given object.
  // This method should not be called with undefined or null.
  static inline Map* GetCodeCacheMapForObject(Object* object);

 protected:
  Address fp() const { return fp_; }
  Address pc() const { return *pc_address_; }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Computes the address in the original code when the code running is
  // containing break points (calls to DebugBreakXXX builtins).
  Address OriginalCodeAddress();
#endif

  // Set the call-site target.
  void set_target(Code* code) { SetTargetAtAddress(address(), code); }

#ifdef DEBUG
  static void TraceIC(const char* type,
                      Handle<String> name,
                      State old_state,
                      Code* new_target,
                      const char* extra_info = "");
#endif

  static Failure* TypeError(const char* type,
                            Handle<Object> object,
                            Handle<String> name);
  static Failure* ReferenceError(const char* type, Handle<String> name);

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


class CallIC: public IC {
 public:
  CallIC() : IC(EXTRA_CALL_FRAME) { ASSERT(target()->is_call_stub()); }

  Object* LoadFunction(State state, Handle<Object> object, Handle<String> name);


  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm, int argc);
  static void GenerateMiss(MacroAssembler* masm, int argc);
  static void GenerateMegamorphic(MacroAssembler* masm, int argc);
  static void GenerateNormal(MacroAssembler* masm, int argc);

 private:
  static void Generate(MacroAssembler* masm,
                       int argc,
                       const ExternalReference& f);

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Handle<Object> object,
                    Handle<String> name);

  // Returns a JSFunction if the object can be called as a function,
  // and patches the stack to be ready for the call.
  // Otherwise, it returns the undefined value.
  Object* TryCallAsFunction(Object* object);

  static void Clear(Address address, Code* target);
  friend class IC;
};


class LoadIC: public IC {
 public:
  LoadIC() : IC(NO_EXTRA_FRAME) { ASSERT(target()->is_load_stub()); }

  Object* Load(State state, Handle<Object> object, Handle<String> name);

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm);
  static void GeneratePreMonomorphic(MacroAssembler* masm);
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm);
  static void GenerateNormal(MacroAssembler* masm);

  // Specialized code generator routines.
  static void GenerateArrayLength(MacroAssembler* masm);
  static void GenerateStringLength(MacroAssembler* masm);
  static void GenerateFunctionPrototype(MacroAssembler* masm);

  // The offset from the inlined patch site to the start of the
  // inlined load instruction.  It is architecture-dependent, and not
  // used on ARM.
  static const int kOffsetToLoadInstruction;

 private:
  static void Generate(MacroAssembler* masm, const ExternalReference& f);

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Handle<Object> object,
                    Handle<String> name);

  // Stub accessors.
  static Code* megamorphic_stub() {
    return Builtins::builtin(Builtins::LoadIC_Megamorphic);
  }
  static Code* initialize_stub() {
    return Builtins::builtin(Builtins::LoadIC_Initialize);
  }
  static Code* pre_monomorphic_stub() {
    return Builtins::builtin(Builtins::LoadIC_PreMonomorphic);
  }

  static void Clear(Address address, Code* target);

  // Clear the use of the inlined version.
  static void ClearInlinedVersion(Address address);

  static bool PatchInlinedLoad(Address address, Object* map, int index);

  friend class IC;
};


class KeyedLoadIC: public IC {
 public:
  KeyedLoadIC() : IC(NO_EXTRA_FRAME) { ASSERT(target()->is_keyed_load_stub()); }

  Object* Load(State state, Handle<Object> object, Handle<Object> key);

  // Code generator routines.
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm);
  static void GeneratePreMonomorphic(MacroAssembler* masm);
  static void GenerateGeneric(MacroAssembler* masm);

  // Clear the use of the inlined version.
  static void ClearInlinedVersion(Address address);

 private:
  static void Generate(MacroAssembler* masm, const ExternalReference& f);

  // Update the inline cache.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Handle<Object> object,
                    Handle<String> name);

  // Stub accessors.
  static Code* initialize_stub() {
    return Builtins::builtin(Builtins::KeyedLoadIC_Initialize);
  }
  static Code* megamorphic_stub() {
    return Builtins::builtin(Builtins::KeyedLoadIC_Generic);
  }
  static Code* generic_stub() {
    return Builtins::builtin(Builtins::KeyedLoadIC_Generic);
  }
  static Code* pre_monomorphic_stub() {
    return Builtins::builtin(Builtins::KeyedLoadIC_PreMonomorphic);
  }

  static void Clear(Address address, Code* target);

  // Support for patching the map that is checked in an inlined
  // version of keyed load.
  static bool PatchInlinedLoad(Address address, Object* map);

  friend class IC;
};


class StoreIC: public IC {
 public:
  StoreIC() : IC(NO_EXTRA_FRAME) { ASSERT(target()->is_store_stub()); }

  Object* Store(State state,
                Handle<Object> object,
                Handle<String> name,
                Handle<Object> value);

  // Code generators for stub routines. Only called once at startup.
  static void GenerateInitialize(MacroAssembler* masm);
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm);
  static void GenerateExtendStorage(MacroAssembler* masm);

 private:
  static void Generate(MacroAssembler* masm, const ExternalReference& f);

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    State state, Handle<JSObject> receiver,
                    Handle<String> name,
                    Handle<Object> value);

  // Stub accessors.
  static Code* megamorphic_stub() {
    return Builtins::builtin(Builtins::StoreIC_Megamorphic);
  }
  static Code* initialize_stub() {
    return Builtins::builtin(Builtins::StoreIC_Initialize);
  }

  static void Clear(Address address, Code* target);
  friend class IC;
};


class KeyedStoreIC: public IC {
 public:
  KeyedStoreIC() : IC(NO_EXTRA_FRAME) { }

  Object* Store(State state,
                Handle<Object> object,
                Handle<Object> name,
                Handle<Object> value);

  // Code generators for stub routines.  Only called once at startup.
  static void GenerateInitialize(MacroAssembler* masm);
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateGeneric(MacroAssembler* masm);
  static void GenerateExtendStorage(MacroAssembler* masm);

  // Clear the inlined version so the IC is always hit.
  static void ClearInlinedVersion(Address address);

  // Restore the inlined version so the fast case can get hit.
  static void RestoreInlinedVersion(Address address);

 private:
  static void Generate(MacroAssembler* masm, const ExternalReference& f);

  // Update the inline cache.
  void UpdateCaches(LookupResult* lookup,
                    State state,
                    Handle<JSObject> receiver,
                    Handle<String> name,
                    Handle<Object> value);

  // Stub accessors.
  static Code* initialize_stub() {
    return Builtins::builtin(Builtins::KeyedStoreIC_Initialize);
  }
  static Code* megamorphic_stub() {
    return Builtins::builtin(Builtins::KeyedStoreIC_Generic);
  }
  static Code* generic_stub() {
    return Builtins::builtin(Builtins::KeyedStoreIC_Generic);
  }

  static void Clear(Address address, Code* target);

  // Support for patching the map that is checked in an inlined
  // version of keyed store.
  // The address is the patch point for the IC call
  // (Assembler::kCallTargetAddressOffset before the end of
  // the call/return address).
  // The map is the new map that the inlined code should check against.
  static bool PatchInlinedStore(Address address, Object* map);

  friend class IC;
};


} }  // namespace v8::internal

#endif  // V8_IC_H_
