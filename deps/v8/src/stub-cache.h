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

#ifndef V8_STUB_CACHE_H_
#define V8_STUB_CACHE_H_

#include "allocation.h"
#include "arguments.h"
#include "code-stubs.h"
#include "ic-inl.h"
#include "macro-assembler.h"
#include "objects.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {


// The stub cache is used for megamorphic calls and property accesses.
// It maps (map, name, type)->Code*

// The design of the table uses the inline cache stubs used for
// mono-morphic calls. The beauty of this, we do not have to
// invalidate the cache whenever a prototype map is changed.  The stub
// validates the map chain as in the mono-morphic case.

class SmallMapList;
class StubCache;


class SCTableReference {
 public:
  Address address() const { return address_; }

 private:
  explicit SCTableReference(Address address) : address_(address) {}

  Address address_;

  friend class StubCache;
};


class StubCache {
 public:
  struct Entry {
    Name* key;
    Code* value;
    Map* map;
  };

  void Initialize();

  Handle<JSObject> StubHolder(Handle<JSObject> receiver,
                              Handle<JSObject> holder);

  Handle<Code> FindIC(Handle<Name> name,
                      Handle<Map> stub_holder_map,
                      Code::Kind kind,
                      Code::StubType type,
                      Code::ExtraICState extra_state = Code::kNoExtraICState);

  Handle<Code> FindIC(Handle<Name> name,
                      Handle<JSObject> stub_holder,
                      Code::Kind kind,
                      Code::StubType type,
                      Code::ExtraICState extra_state = Code::kNoExtraICState);

  Handle<Code> FindHandler(
      Handle<Name> name,
      Handle<JSObject> receiver,
      Handle<JSObject> stub_holder,
      Code::Kind kind,
      Code::StubType type);

  Handle<Code> ComputeMonomorphicIC(Handle<JSObject> receiver,
                                    Handle<Code> handler,
                                    Handle<Name> name);
  Handle<Code> ComputeKeyedMonomorphicIC(Handle<JSObject> receiver,
                                         Handle<Code> handler,
                                         Handle<Name> name);

  // Computes the right stub matching. Inserts the result in the
  // cache before returning.  This might compile a stub if needed.
  Handle<Code> ComputeLoadNonexistent(Handle<Name> name,
                                      Handle<JSObject> object);

  Handle<Code> ComputeLoadField(Handle<Name> name,
                                Handle<JSObject> object,
                                Handle<JSObject> holder,
                                PropertyIndex field_index,
                                Representation representation);

  Handle<Code> ComputeLoadCallback(Handle<Name> name,
                                   Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<ExecutableAccessorInfo> callback);

  Handle<Code> ComputeLoadViaGetter(Handle<Name> name,
                                    Handle<JSObject> object,
                                    Handle<JSObject> holder,
                                    Handle<JSFunction> getter);

  Handle<Code> ComputeLoadConstant(Handle<Name> name,
                                   Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<JSFunction> value);

  Handle<Code> ComputeLoadInterceptor(Handle<Name> name,
                                      Handle<JSObject> object,
                                      Handle<JSObject> holder);

  Handle<Code> ComputeLoadNormal(Handle<Name> name,
                                 Handle<JSObject> object);

  Handle<Code> ComputeLoadGlobal(Handle<Name> name,
                                 Handle<JSObject> object,
                                 Handle<GlobalObject> holder,
                                 Handle<JSGlobalPropertyCell> cell,
                                 bool is_dont_delete);

  // ---

  Handle<Code> ComputeKeyedLoadField(Handle<Name> name,
                                     Handle<JSObject> object,
                                     Handle<JSObject> holder,
                                     PropertyIndex field_index,
                                     Representation representation);

  Handle<Code> ComputeKeyedLoadCallback(
      Handle<Name> name,
      Handle<JSObject> object,
      Handle<JSObject> holder,
      Handle<ExecutableAccessorInfo> callback);

  Handle<Code> ComputeKeyedLoadConstant(Handle<Name> name,
                                        Handle<JSObject> object,
                                        Handle<JSObject> holder,
                                        Handle<JSFunction> value);

  Handle<Code> ComputeKeyedLoadInterceptor(Handle<Name> name,
                                           Handle<JSObject> object,
                                           Handle<JSObject> holder);

  // ---

  Handle<Code> ComputeStoreField(Handle<Name> name,
                                 Handle<JSObject> object,
                                 LookupResult* lookup,
                                 StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreTransition(Handle<Name> name,
                                      Handle<JSObject> object,
                                      LookupResult* lookup,
                                      Handle<Map> transition,
                                      StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreNormal(StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreGlobal(Handle<Name> name,
                                  Handle<GlobalObject> object,
                                  Handle<JSGlobalPropertyCell> cell,
                                  StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreCallback(Handle<Name> name,
                                    Handle<JSObject> object,
                                    Handle<JSObject> holder,
                                    Handle<ExecutableAccessorInfo> callback,
                                    StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreViaSetter(Handle<Name> name,
                                     Handle<JSObject> object,
                                     Handle<JSObject> holder,
                                     Handle<JSFunction> setter,
                                     StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreInterceptor(Handle<Name> name,
                                       Handle<JSObject> object,
                                       StrictModeFlag strict_mode);

  // ---

  Handle<Code> ComputeKeyedStoreField(Handle<Name> name,
                                      Handle<JSObject> object,
                                      LookupResult* lookup,
                                      StrictModeFlag strict_mode);
  Handle<Code> ComputeKeyedStoreTransition(Handle<Name> name,
                                           Handle<JSObject> object,
                                           LookupResult* lookup,
                                           Handle<Map> transition,
                                           StrictModeFlag strict_mode);

  Handle<Code> ComputeKeyedLoadElement(Handle<Map> receiver_map);

  Handle<Code> ComputeKeyedStoreElement(Handle<Map> receiver_map,
                                        StrictModeFlag strict_mode,
                                        KeyedAccessStoreMode store_mode);

  // ---

  Handle<Code> ComputeCallField(int argc,
                                Code::Kind,
                                Code::ExtraICState extra_state,
                                Handle<Name> name,
                                Handle<Object> object,
                                Handle<JSObject> holder,
                                PropertyIndex index);

  Handle<Code> ComputeCallConstant(int argc,
                                   Code::Kind,
                                   Code::ExtraICState extra_state,
                                   Handle<Name> name,
                                   Handle<Object> object,
                                   Handle<JSObject> holder,
                                   Handle<JSFunction> function);

  Handle<Code> ComputeCallInterceptor(int argc,
                                      Code::Kind,
                                      Code::ExtraICState extra_state,
                                      Handle<Name> name,
                                      Handle<Object> object,
                                      Handle<JSObject> holder);

  Handle<Code> ComputeCallGlobal(int argc,
                                 Code::Kind,
                                 Code::ExtraICState extra_state,
                                 Handle<Name> name,
                                 Handle<JSObject> object,
                                 Handle<GlobalObject> holder,
                                 Handle<JSGlobalPropertyCell> cell,
                                 Handle<JSFunction> function);

  // ---

  Handle<Code> ComputeCallInitialize(int argc, RelocInfo::Mode mode);

  Handle<Code> ComputeKeyedCallInitialize(int argc);

  Handle<Code> ComputeCallPreMonomorphic(int argc,
                                         Code::Kind kind,
                                         Code::ExtraICState extra_state);

  Handle<Code> ComputeCallNormal(int argc,
                                 Code::Kind kind,
                                 Code::ExtraICState state);

  Handle<Code> ComputeCallArguments(int argc);

  Handle<Code> ComputeCallMegamorphic(int argc,
                                      Code::Kind kind,
                                      Code::ExtraICState state);

  Handle<Code> ComputeCallMiss(int argc,
                               Code::Kind kind,
                               Code::ExtraICState state);

  // ---

  Handle<Code> ComputeCompareNil(Handle<Map> receiver_map,
                                 CompareNilICStub& stub);

  // ---

  Handle<Code> ComputeLoadElementPolymorphic(MapHandleList* receiver_maps);
  Handle<Code> ComputeStoreElementPolymorphic(MapHandleList* receiver_maps,
                                              KeyedAccessStoreMode store_mode,
                                              StrictModeFlag strict_mode);

  Handle<Code> ComputePolymorphicIC(MapHandleList* receiver_maps,
                                    CodeHandleList* handlers,
                                    int number_of_valid_maps,
                                    Handle<Name> name);

  // Finds the Code object stored in the Heap::non_monomorphic_cache().
  Code* FindCallInitialize(int argc, RelocInfo::Mode mode, Code::Kind kind);

#ifdef ENABLE_DEBUGGER_SUPPORT
  Handle<Code> ComputeCallDebugBreak(int argc, Code::Kind kind);

  Handle<Code> ComputeCallDebugPrepareStepIn(int argc, Code::Kind kind);
#endif

  // Update cache for entry hash(name, map).
  Code* Set(Name* name, Map* map, Code* code);

  // Clear the lookup table (@ mark compact collection).
  void Clear();

  // Collect all maps that match the name and flags.
  void CollectMatchingMaps(SmallMapList* types,
                           Handle<Name> name,
                           Code::Flags flags,
                           Handle<Context> native_context,
                           Zone* zone);

  // Generate code for probing the stub cache table.
  // Arguments extra, extra2 and extra3 may be used to pass additional scratch
  // registers. Set to no_reg if not needed.
  void GenerateProbe(MacroAssembler* masm,
                     Code::Flags flags,
                     Register receiver,
                     Register name,
                     Register scratch,
                     Register extra,
                     Register extra2 = no_reg,
                     Register extra3 = no_reg);

  enum Table {
    kPrimary,
    kSecondary
  };


  SCTableReference key_reference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->key));
  }


  SCTableReference map_reference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->map));
  }


  SCTableReference value_reference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->value));
  }


  StubCache::Entry* first_entry(StubCache::Table table) {
    switch (table) {
      case StubCache::kPrimary: return StubCache::primary_;
      case StubCache::kSecondary: return StubCache::secondary_;
    }
    UNREACHABLE();
    return NULL;
  }

  Isolate* isolate() { return isolate_; }
  Heap* heap() { return isolate()->heap(); }
  Factory* factory() { return isolate()->factory(); }

 private:
  StubCache(Isolate* isolate, Zone* zone);

  Handle<Code> ComputeCallInitialize(int argc,
                                     RelocInfo::Mode mode,
                                     Code::Kind kind);

  // The stub cache has a primary and secondary level.  The two levels have
  // different hashing algorithms in order to avoid simultaneous collisions
  // in both caches.  Unlike a probing strategy (quadratic or otherwise) the
  // update strategy on updates is fairly clear and simple:  Any existing entry
  // in the primary cache is moved to the secondary cache, and secondary cache
  // entries are overwritten.

  // Hash algorithm for the primary table.  This algorithm is replicated in
  // assembler for every architecture.  Returns an index into the table that
  // is scaled by 1 << kHeapObjectTagSize.
  static int PrimaryOffset(Name* name, Code::Flags flags, Map* map) {
    // This works well because the heap object tag size and the hash
    // shift are equal.  Shifting down the length field to get the
    // hash code would effectively throw away two bits of the hash
    // code.
    STATIC_ASSERT(kHeapObjectTagSize == Name::kHashShift);
    // Compute the hash of the name (use entire hash field).
    ASSERT(name->HasHashCode());
    uint32_t field = name->hash_field();
    // Using only the low bits in 64-bit mode is unlikely to increase the
    // risk of collision even if the heap is spread over an area larger than
    // 4Gb (and not at all if it isn't).
    uint32_t map_low32bits =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(map));
    // We always set the in_loop bit to zero when generating the lookup code
    // so do it here too so the hash codes match.
    uint32_t iflags =
        (static_cast<uint32_t>(flags) & ~Code::kFlagsNotUsedInLookup);
    // Base the offset on a simple combination of name, flags, and map.
    uint32_t key = (map_low32bits + field) ^ iflags;
    return key & ((kPrimaryTableSize - 1) << kHeapObjectTagSize);
  }

  // Hash algorithm for the secondary table.  This algorithm is replicated in
  // assembler for every architecture.  Returns an index into the table that
  // is scaled by 1 << kHeapObjectTagSize.
  static int SecondaryOffset(Name* name, Code::Flags flags, int seed) {
    // Use the seed from the primary cache in the secondary cache.
    uint32_t name_low32bits =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name));
    // We always set the in_loop bit to zero when generating the lookup code
    // so do it here too so the hash codes match.
    uint32_t iflags =
        (static_cast<uint32_t>(flags) & ~Code::kFlagsNotUsedInLookup);
    uint32_t key = (seed - name_low32bits) + iflags;
    return key & ((kSecondaryTableSize - 1) << kHeapObjectTagSize);
  }

  // Compute the entry for a given offset in exactly the same way as
  // we do in generated code.  We generate an hash code that already
  // ends in Name::kHashShift 0s.  Then we multiply it so it is a multiple
  // of sizeof(Entry).  This makes it easier to avoid making mistakes
  // in the hashed offset computations.
  static Entry* entry(Entry* table, int offset) {
    const int multiplier = sizeof(*table) >> Name::kHashShift;
    return reinterpret_cast<Entry*>(
        reinterpret_cast<Address>(table) + offset * multiplier);
  }

  static const int kPrimaryTableBits = 11;
  static const int kPrimaryTableSize = (1 << kPrimaryTableBits);
  static const int kSecondaryTableBits = 9;
  static const int kSecondaryTableSize = (1 << kSecondaryTableBits);

  Entry primary_[kPrimaryTableSize];
  Entry secondary_[kSecondaryTableSize];
  Isolate* isolate_;

  friend class Isolate;
  friend class SCTableReference;

  DISALLOW_COPY_AND_ASSIGN(StubCache);
};


// ------------------------------------------------------------------------


// Support functions for IC stubs for callbacks.
DECLARE_RUNTIME_FUNCTION(MaybeObject*, StoreCallbackProperty);


// Support functions for IC stubs for interceptors.
DECLARE_RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorOnly);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorForLoad);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorForCall);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, StoreInterceptorProperty);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, CallInterceptorProperty);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, KeyedLoadPropertyWithInterceptor);


enum PrototypeCheckType { CHECK_ALL_MAPS, SKIP_RECEIVER };
enum IcCheckType { ELEMENT, PROPERTY };


// The stub compilers compile stubs for the stub cache.
class StubCompiler BASE_EMBEDDED {
 public:
  explicit StubCompiler(Isolate* isolate)
      : isolate_(isolate), masm_(isolate, NULL, 256), failure_(NULL) { }

  // Functions to compile either CallIC or KeyedCallIC.  The specific kind
  // is extracted from the code flags.
  Handle<Code> CompileCallInitialize(Code::Flags flags);
  Handle<Code> CompileCallPreMonomorphic(Code::Flags flags);
  Handle<Code> CompileCallNormal(Code::Flags flags);
  Handle<Code> CompileCallMegamorphic(Code::Flags flags);
  Handle<Code> CompileCallArguments(Code::Flags flags);
  Handle<Code> CompileCallMiss(Code::Flags flags);

#ifdef ENABLE_DEBUGGER_SUPPORT
  Handle<Code> CompileCallDebugBreak(Code::Flags flags);
  Handle<Code> CompileCallDebugPrepareStepIn(Code::Flags flags);
#endif

  // Static functions for generating parts of stubs.
  static void GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                  int index,
                                                  Register prototype);

  // Generates prototype loading code that uses the objects from the
  // context we were in when this function was called. If the context
  // has changed, a jump to miss is performed. This ties the generated
  // code to a particular context and so must not be used in cases
  // where the generated code is not allowed to have references to
  // objects from a context.
  static void GenerateDirectLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                        int index,
                                                        Register prototype,
                                                        Label* miss);

  static void GenerateFastPropertyLoad(MacroAssembler* masm,
                                       Register dst,
                                       Register src,
                                       bool inobject,
                                       int index,
                                       Representation representation);

  static void GenerateLoadArrayLength(MacroAssembler* masm,
                                      Register receiver,
                                      Register scratch,
                                      Label* miss_label);

  static void GenerateLoadStringLength(MacroAssembler* masm,
                                       Register receiver,
                                       Register scratch1,
                                       Register scratch2,
                                       Label* miss_label,
                                       bool support_wrappers);

  static void GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                            Register receiver,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss_label);

  void GenerateStoreTransition(MacroAssembler* masm,
                               Handle<JSObject> object,
                               LookupResult* lookup,
                               Handle<Map> transition,
                               Handle<Name> name,
                               Register receiver_reg,
                               Register name_reg,
                               Register value_reg,
                               Register scratch1,
                               Register scratch2,
                               Register scratch3,
                               Label* miss_label,
                               Label* miss_restore_name,
                               Label* slow);

  void GenerateStoreField(MacroAssembler* masm,
                          Handle<JSObject> object,
                          LookupResult* lookup,
                          Register receiver_reg,
                          Register name_reg,
                          Register value_reg,
                          Register scratch1,
                          Register scratch2,
                          Label* miss_label);

  static Builtins::Name MissBuiltin(Code::Kind kind) {
    switch (kind) {
      case Code::LOAD_IC: return Builtins::kLoadIC_Miss;
      case Code::STORE_IC: return Builtins::kStoreIC_Miss;
      case Code::KEYED_LOAD_IC: return Builtins::kKeyedLoadIC_Miss;
      case Code::KEYED_STORE_IC: return Builtins::kKeyedStoreIC_Miss;
      default: UNREACHABLE();
    }
    return Builtins::kLoadIC_Miss;
  }
  static Builtins::Name SlowBuiltin(Code::Kind kind) {
    switch (kind) {
      case Code::STORE_IC: return Builtins::kStoreIC_Slow;
      case Code::KEYED_STORE_IC: return Builtins::kKeyedStoreIC_Slow;
      default: UNREACHABLE();
    }
    return Builtins::kStoreIC_Slow;
  }
  static void TailCallBuiltin(MacroAssembler* masm, Builtins::Name name);

  // Generates code that verifies that the property holder has not changed
  // (checking maps of objects in the prototype chain for fast and global
  // objects or doing negative lookup for slow objects, ensures that the
  // property cells for global objects are still empty) and checks that the map
  // of the holder has not changed. If necessary the function also generates
  // code for security check in case of global object holders. Helps to make
  // sure that the current IC is still valid.
  //
  // The scratch and holder registers are always clobbered, but the object
  // register is only clobbered if it the same as the holder register. The
  // function returns a register containing the holder - either object_reg or
  // holder_reg.
  // The function can optionally (when save_at_depth !=
  // kInvalidProtoDepth) save the object at the given depth by moving
  // it to [esp + kPointerSize].
  Register CheckPrototypes(Handle<JSObject> object,
                           Register object_reg,
                           Handle<JSObject> holder,
                           Register holder_reg,
                           Register scratch1,
                           Register scratch2,
                           Handle<Name> name,
                           Label* miss,
                           PrototypeCheckType check = CHECK_ALL_MAPS) {
    return CheckPrototypes(object, object_reg, holder, holder_reg, scratch1,
                           scratch2, name, kInvalidProtoDepth, miss, check);
  }

  Register CheckPrototypes(Handle<JSObject> object,
                           Register object_reg,
                           Handle<JSObject> holder,
                           Register holder_reg,
                           Register scratch1,
                           Register scratch2,
                           Handle<Name> name,
                           int save_at_depth,
                           Label* miss,
                           PrototypeCheckType check = CHECK_ALL_MAPS);


 protected:
  Handle<Code> GetCodeWithFlags(Code::Flags flags, const char* name);
  Handle<Code> GetCodeWithFlags(Code::Flags flags, Handle<Name> name);

  MacroAssembler* masm() { return &masm_; }
  void set_failure(Failure* failure) { failure_ = failure; }

  static void LookupPostInterceptor(Handle<JSObject> holder,
                                    Handle<Name> name,
                                    LookupResult* lookup);

  Isolate* isolate() { return isolate_; }
  Heap* heap() { return isolate()->heap(); }
  Factory* factory() { return isolate()->factory(); }

  static void GenerateTailCall(MacroAssembler* masm, Handle<Code> code);

 private:
  Isolate* isolate_;
  MacroAssembler masm_;
  Failure* failure_;
};


enum FrontendCheckType { PERFORM_INITIAL_CHECKS, SKIP_INITIAL_CHECKS };


class BaseLoadStubCompiler: public StubCompiler {
 public:
  BaseLoadStubCompiler(Isolate* isolate, Register* registers)
      : StubCompiler(isolate), registers_(registers) { }
  virtual ~BaseLoadStubCompiler() { }

  Handle<Code> CompileLoadField(Handle<JSObject> object,
                                Handle<JSObject> holder,
                                Handle<Name> name,
                                PropertyIndex index,
                                Representation representation);

  Handle<Code> CompileLoadCallback(Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   Handle<ExecutableAccessorInfo> callback);

  Handle<Code> CompileLoadConstant(Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   Handle<JSFunction> value);

  Handle<Code> CompileLoadInterceptor(Handle<JSObject> object,
                                      Handle<JSObject> holder,
                                      Handle<Name> name);

  Handle<Code> CompileMonomorphicIC(Handle<Map> receiver_map,
                                    Handle<Code> handler,
                                    Handle<Name> name);
  Handle<Code> CompilePolymorphicIC(MapHandleList* receiver_maps,
                                    CodeHandleList* handlers,
                                    Handle<Name> name,
                                    Code::StubType type,
                                    IcCheckType check);

 protected:
  Register HandlerFrontendHeader(Handle<JSObject> object,
                                 Register object_reg,
                                 Handle<JSObject> holder,
                                 Handle<Name> name,
                                 Label* success);
  void HandlerFrontendFooter(Label* success, Label* miss);

  Register HandlerFrontend(Handle<JSObject> object,
                           Register object_reg,
                           Handle<JSObject> holder,
                           Handle<Name> name,
                           Label* success);
  Register CallbackHandlerFrontend(Handle<JSObject> object,
                                   Register object_reg,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   Label* success,
                                   Handle<ExecutableAccessorInfo> callback);
  void NonexistentHandlerFrontend(Handle<JSObject> object,
                                  Handle<JSObject> last,
                                  Handle<Name> name,
                                  Label* success,
                                  Handle<GlobalObject> global);

  void GenerateLoadField(Register reg,
                         Handle<JSObject> holder,
                         PropertyIndex field,
                         Representation representation);
  void GenerateLoadConstant(Handle<JSFunction> value);
  void GenerateLoadCallback(Register reg,
                            Handle<ExecutableAccessorInfo> callback);
  void GenerateLoadInterceptor(Register holder_reg,
                               Handle<JSObject> object,
                               Handle<JSObject> holder,
                               LookupResult* lookup,
                               Handle<Name> name);
  void GenerateLoadPostInterceptor(Register reg,
                                   Handle<JSObject> interceptor_holder,
                                   Handle<Name> name,
                                   LookupResult* lookup);

  Handle<Code> GetICCode(Code::Kind kind,
                         Code::StubType type,
                         Handle<Name> name,
                         InlineCacheState state = MONOMORPHIC);

  Handle<Code> GetCode(Code::Kind kind,
                       Code::StubType type,
                       Handle<Name> name);

  Register receiver() { return registers_[0]; }
  Register name()     { return registers_[1]; }
  Register scratch1() { return registers_[2]; }
  Register scratch2() { return registers_[3]; }
  Register scratch3() { return registers_[4]; }
  Register scratch4() { return registers_[5]; }

 private:
  virtual Code::Kind kind() = 0;
  virtual Logger::LogEventsAndTags log_kind(Handle<Code> code) = 0;
  virtual void JitEvent(Handle<Name> name, Handle<Code> code) = 0;
  virtual void GenerateNameCheck(Handle<Name> name,
                                 Register name_reg,
                                 Label* miss) { }
  Register* registers_;
};


class LoadStubCompiler: public BaseLoadStubCompiler {
 public:
  explicit LoadStubCompiler(Isolate* isolate)
      : BaseLoadStubCompiler(isolate, registers()) { }

  Handle<Code> CompileLoadNonexistent(Handle<JSObject> object,
                                      Handle<JSObject> last,
                                      Handle<Name> name,
                                      Handle<GlobalObject> global);

  static void GenerateLoadViaGetter(MacroAssembler* masm,
                                    Handle<JSFunction> getter);

  Handle<Code> CompileLoadViaGetter(Handle<JSObject> object,
                                    Handle<JSObject> holder,
                                    Handle<Name> name,
                                    Handle<JSFunction> getter);

  Handle<Code> CompileLoadGlobal(Handle<JSObject> object,
                                 Handle<GlobalObject> holder,
                                 Handle<JSGlobalPropertyCell> cell,
                                 Handle<Name> name,
                                 bool is_dont_delete);

  static Register receiver() { return registers()[0]; }

 private:
  static Register* registers();
  virtual Code::Kind kind() { return Code::LOAD_IC; }
  virtual Logger::LogEventsAndTags log_kind(Handle<Code> code) {
    if (!code->is_inline_cache_stub()) return Logger::STUB_TAG;
    return code->ic_state() == MONOMORPHIC
        ? Logger::LOAD_IC_TAG : Logger::LOAD_POLYMORPHIC_IC_TAG;
  }
  virtual void JitEvent(Handle<Name> name, Handle<Code> code);
};


class KeyedLoadStubCompiler: public BaseLoadStubCompiler {
 public:
  explicit KeyedLoadStubCompiler(Isolate* isolate)
      : BaseLoadStubCompiler(isolate, registers()) { }

  Handle<Code> CompileLoadElement(Handle<Map> receiver_map);

  void CompileElementHandlers(MapHandleList* receiver_maps,
                              CodeHandleList* handlers);

  static void GenerateLoadDictionaryElement(MacroAssembler* masm);

  static Register receiver() { return registers()[0]; }

 private:
  static Register* registers();
  virtual Code::Kind kind() { return Code::KEYED_LOAD_IC; }
  virtual Logger::LogEventsAndTags log_kind(Handle<Code> code) {
    if (!code->is_inline_cache_stub()) return Logger::STUB_TAG;
    return code->ic_state() == MONOMORPHIC
        ? Logger::KEYED_LOAD_IC_TAG : Logger::KEYED_LOAD_POLYMORPHIC_IC_TAG;
  }
  virtual void JitEvent(Handle<Name> name, Handle<Code> code);
  virtual void GenerateNameCheck(Handle<Name> name,
                                 Register name_reg,
                                 Label* miss);
};


class BaseStoreStubCompiler: public StubCompiler {
 public:
  BaseStoreStubCompiler(Isolate* isolate,
                        StrictModeFlag strict_mode,
                        Register* registers)
      : StubCompiler(isolate),
        strict_mode_(strict_mode),
        registers_(registers) { }

  virtual ~BaseStoreStubCompiler() { }

  Handle<Code> CompileStoreTransition(Handle<JSObject> object,
                                      LookupResult* lookup,
                                      Handle<Map> transition,
                                      Handle<Name> name);

  Handle<Code> CompileStoreField(Handle<JSObject> object,
                                 LookupResult* lookup,
                                 Handle<Name> name);

 protected:
  Handle<Code> GetICCode(Code::Kind kind,
                         Code::StubType type,
                         Handle<Name> name,
                         InlineCacheState state = MONOMORPHIC);

  Handle<Code> GetCode(Code::Kind kind,
                       Code::StubType type,
                       Handle<Name> name);

  void GenerateRestoreName(MacroAssembler* masm,
                           Label* label,
                           Handle<Name> name);

  Register receiver() { return registers_[0]; }
  Register name()     { return registers_[1]; }
  Register value()    { return registers_[2]; }
  Register scratch1() { return registers_[3]; }
  Register scratch2() { return registers_[4]; }
  Register scratch3() { return registers_[5]; }
  StrictModeFlag strict_mode() { return strict_mode_; }
  virtual Code::ExtraICState extra_state() { return strict_mode_; }

 private:
  virtual Code::Kind kind() = 0;
  virtual Logger::LogEventsAndTags log_kind(Handle<Code> code) = 0;
  virtual void JitEvent(Handle<Name> name, Handle<Code> code) = 0;
  virtual void GenerateNameCheck(Handle<Name> name,
                                 Register name_reg,
                                 Label* miss) { }
  StrictModeFlag strict_mode_;
  Register* registers_;
};


class StoreStubCompiler: public BaseStoreStubCompiler {
 public:
  StoreStubCompiler(Isolate* isolate, StrictModeFlag strict_mode)
      : BaseStoreStubCompiler(isolate, strict_mode, registers()) { }


  Handle<Code> CompileStoreCallback(Handle<Name> name,
                                    Handle<JSObject> object,
                                    Handle<JSObject> holder,
                                    Handle<ExecutableAccessorInfo> callback);

  static void GenerateStoreViaSetter(MacroAssembler* masm,
                                     Handle<JSFunction> setter);

  Handle<Code> CompileStoreViaSetter(Handle<Name> name,
                                     Handle<JSObject> object,
                                     Handle<JSObject> holder,
                                     Handle<JSFunction> setter);

  Handle<Code> CompileStoreInterceptor(Handle<JSObject> object,
                                       Handle<Name> name);

  Handle<Code> CompileStoreGlobal(Handle<GlobalObject> object,
                                  Handle<JSGlobalPropertyCell> holder,
                                  Handle<Name> name);

 private:
  static Register* registers();
  virtual Code::Kind kind() { return Code::STORE_IC; }
  virtual Logger::LogEventsAndTags log_kind(Handle<Code> code) {
    if (!code->is_inline_cache_stub()) return Logger::STUB_TAG;
    return code->ic_state() == MONOMORPHIC
        ? Logger::STORE_IC_TAG : Logger::STORE_POLYMORPHIC_IC_TAG;
  }
  virtual void JitEvent(Handle<Name> name, Handle<Code> code);
};


class KeyedStoreStubCompiler: public BaseStoreStubCompiler {
 public:
  KeyedStoreStubCompiler(Isolate* isolate,
                         StrictModeFlag strict_mode,
                         KeyedAccessStoreMode store_mode)
      : BaseStoreStubCompiler(isolate, strict_mode, registers()),
        store_mode_(store_mode) { }

  Handle<Code> CompileStoreElement(Handle<Map> receiver_map);

  Handle<Code> CompileStorePolymorphic(MapHandleList* receiver_maps,
                                       CodeHandleList* handler_stubs,
                                       MapHandleList* transitioned_maps);

  Handle<Code> CompileStoreElementPolymorphic(MapHandleList* receiver_maps);

  static void GenerateStoreFastElement(MacroAssembler* masm,
                                       bool is_js_array,
                                       ElementsKind element_kind,
                                       KeyedAccessStoreMode store_mode);

  static void GenerateStoreFastDoubleElement(MacroAssembler* masm,
                                             bool is_js_array,
                                             KeyedAccessStoreMode store_mode);

  static void GenerateStoreExternalArray(MacroAssembler* masm,
                                         ElementsKind elements_kind);

  static void GenerateStoreDictionaryElement(MacroAssembler* masm);

 protected:
  virtual Code::ExtraICState extra_state() {
    return Code::ComputeExtraICState(store_mode_, strict_mode());
  }

 private:
  Register transition_map() {
    return registers()[3];
  }

  static Register* registers();
  virtual Code::Kind kind() { return Code::KEYED_STORE_IC; }
  virtual Logger::LogEventsAndTags log_kind(Handle<Code> code) {
    if (!code->is_inline_cache_stub()) return Logger::STUB_TAG;
    return code->ic_state() == MONOMORPHIC
        ? Logger::KEYED_STORE_IC_TAG : Logger::KEYED_STORE_POLYMORPHIC_IC_TAG;
  }
  virtual void JitEvent(Handle<Name> name, Handle<Code> code);
  virtual void GenerateNameCheck(Handle<Name> name,
                                 Register name_reg,
                                 Label* miss);
  KeyedAccessStoreMode store_mode_;
};


// Subset of FUNCTIONS_WITH_ID_LIST with custom constant/global call
// IC stubs.
#define CUSTOM_CALL_IC_GENERATORS(V)            \
  V(ArrayPush)                                  \
  V(ArrayPop)                                   \
  V(StringCharCodeAt)                           \
  V(StringCharAt)                               \
  V(StringFromCharCode)                         \
  V(MathFloor)                                  \
  V(MathAbs)


class CallOptimization;

class CallStubCompiler: public StubCompiler {
 public:
  CallStubCompiler(Isolate* isolate,
                   int argc,
                   Code::Kind kind,
                   Code::ExtraICState extra_state,
                   InlineCacheHolderFlag cache_holder);

  Handle<Code> CompileCallField(Handle<JSObject> object,
                                Handle<JSObject> holder,
                                PropertyIndex index,
                                Handle<Name> name);

  void CompileHandlerFrontend(Handle<Object> object,
                              Handle<JSObject> holder,
                              Handle<Name> name,
                              CheckType check,
                              Label* success);

  void CompileHandlerBackend(Handle<JSFunction> function);

  Handle<Code> CompileCallConstant(Handle<Object> object,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   CheckType check,
                                   Handle<JSFunction> function);

  Handle<Code> CompileCallInterceptor(Handle<JSObject> object,
                                      Handle<JSObject> holder,
                                      Handle<Name> name);

  Handle<Code> CompileCallGlobal(Handle<JSObject> object,
                                 Handle<GlobalObject> holder,
                                 Handle<JSGlobalPropertyCell> cell,
                                 Handle<JSFunction> function,
                                 Handle<Name> name);

  static bool HasCustomCallGenerator(Handle<JSFunction> function);

 private:
  // Compiles a custom call constant/global IC.  For constant calls cell is
  // NULL.  Returns an empty handle if there is no custom call code for the
  // given function.
  Handle<Code> CompileCustomCall(Handle<Object> object,
                                 Handle<JSObject> holder,
                                 Handle<JSGlobalPropertyCell> cell,
                                 Handle<JSFunction> function,
                                 Handle<String> name);

#define DECLARE_CALL_GENERATOR(name)                                    \
  Handle<Code> Compile##name##Call(Handle<Object> object,               \
                                   Handle<JSObject> holder,             \
                                   Handle<JSGlobalPropertyCell> cell,   \
                                   Handle<JSFunction> function,         \
                                   Handle<String> fname);
  CUSTOM_CALL_IC_GENERATORS(DECLARE_CALL_GENERATOR)
#undef DECLARE_CALL_GENERATOR

  Handle<Code> CompileFastApiCall(const CallOptimization& optimization,
                                  Handle<Object> object,
                                  Handle<JSObject> holder,
                                  Handle<JSGlobalPropertyCell> cell,
                                  Handle<JSFunction> function,
                                  Handle<String> name);

  Handle<Code> GetCode(Code::StubType type, Handle<Name> name);
  Handle<Code> GetCode(Handle<JSFunction> function);

  const ParameterCount& arguments() { return arguments_; }

  void GenerateNameCheck(Handle<Name> name, Label* miss);

  void GenerateGlobalReceiverCheck(Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   Label* miss);

  // Generates code to load the function from the cell checking that
  // it still contains the same function.
  void GenerateLoadFunctionFromCell(Handle<JSGlobalPropertyCell> cell,
                                    Handle<JSFunction> function,
                                    Label* miss);

  // Generates a jump to CallIC miss stub.
  void GenerateMissBranch();

  const ParameterCount arguments_;
  const Code::Kind kind_;
  const Code::ExtraICState extra_state_;
  const InlineCacheHolderFlag cache_holder_;
};


class ConstructStubCompiler: public StubCompiler {
 public:
  explicit ConstructStubCompiler(Isolate* isolate) : StubCompiler(isolate) { }

  Handle<Code> CompileConstructStub(Handle<JSFunction> function);

 private:
  Handle<Code> GetCode();
};


// Holds information about possible function call optimizations.
class CallOptimization BASE_EMBEDDED {
 public:
  explicit CallOptimization(LookupResult* lookup);

  explicit CallOptimization(Handle<JSFunction> function);

  bool is_constant_call() const {
    return !constant_function_.is_null();
  }

  Handle<JSFunction> constant_function() const {
    ASSERT(is_constant_call());
    return constant_function_;
  }

  bool is_simple_api_call() const {
    return is_simple_api_call_;
  }

  Handle<FunctionTemplateInfo> expected_receiver_type() const {
    ASSERT(is_simple_api_call());
    return expected_receiver_type_;
  }

  Handle<CallHandlerInfo> api_call_info() const {
    ASSERT(is_simple_api_call());
    return api_call_info_;
  }

  // Returns the depth of the object having the expected type in the
  // prototype chain between the two arguments.
  int GetPrototypeDepthOfExpectedType(Handle<JSObject> object,
                                      Handle<JSObject> holder) const;

 private:
  void Initialize(Handle<JSFunction> function);

  // Determines whether the given function can be called using the
  // fast api call builtin.
  void AnalyzePossibleApiFunction(Handle<JSFunction> function);

  Handle<JSFunction> constant_function_;
  bool is_simple_api_call_;
  Handle<FunctionTemplateInfo> expected_receiver_type_;
  Handle<CallHandlerInfo> api_call_info_;
};


} }  // namespace v8::internal

#endif  // V8_STUB_CACHE_H_
