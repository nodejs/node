// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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


class CallOptimization;
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
                      ExtraICState extra_state = kNoExtraICState,
                      InlineCacheHolderFlag cache_holder = OWN_MAP);

  Handle<Code> FindHandler(Handle<Name> name,
                           Handle<Map> map,
                           Code::Kind kind,
                           InlineCacheHolderFlag cache_holder,
                           Code::StubType type);

  Handle<Code> ComputeMonomorphicIC(Code::Kind kind,
                                    Handle<Name> name,
                                    Handle<HeapType> type,
                                    Handle<Code> handler,
                                    ExtraICState extra_ic_state);

  Handle<Code> ComputeLoadNonexistent(Handle<Name> name, Handle<HeapType> type);

  Handle<Code> ComputeKeyedLoadElement(Handle<Map> receiver_map);

  Handle<Code> ComputeKeyedStoreElement(Handle<Map> receiver_map,
                                        StrictMode strict_mode,
                                        KeyedAccessStoreMode store_mode);

  // ---

  Handle<Code> ComputeLoad(InlineCacheState ic_state, ExtraICState extra_state);
  Handle<Code> ComputeStore(InlineCacheState ic_state,
                            ExtraICState extra_state);

  // ---

  Handle<Code> ComputeCompareNil(Handle<Map> receiver_map,
                                 CompareNilICStub& stub);

  // ---

  Handle<Code> ComputeLoadElementPolymorphic(MapHandleList* receiver_maps);
  Handle<Code> ComputeStoreElementPolymorphic(MapHandleList* receiver_maps,
                                              KeyedAccessStoreMode store_mode,
                                              StrictMode strict_mode);

  Handle<Code> ComputePolymorphicIC(Code::Kind kind,
                                    TypeHandleList* types,
                                    CodeHandleList* handlers,
                                    int number_of_valid_maps,
                                    Handle<Name> name,
                                    ExtraICState extra_ic_state);

  // Finds the Code object stored in the Heap::non_monomorphic_cache().
  Code* FindPreMonomorphicIC(Code::Kind kind, ExtraICState extra_ic_state);

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

  // These constants describe the structure of the interceptor arguments on the
  // stack. The arguments are pushed by the (platform-specific)
  // PushInterceptorArguments and read by LoadPropertyWithInterceptorOnly and
  // LoadWithInterceptor.
  static const int kInterceptorArgsNameIndex = 0;
  static const int kInterceptorArgsInfoIndex = 1;
  static const int kInterceptorArgsThisIndex = 2;
  static const int kInterceptorArgsHolderIndex = 3;
  static const int kInterceptorArgsLength = 4;

 private:
  explicit StubCache(Isolate* isolate);

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
DECLARE_RUNTIME_FUNCTION(StoreCallbackProperty);


// Support functions for IC stubs for interceptors.
DECLARE_RUNTIME_FUNCTION(LoadPropertyWithInterceptorOnly);
DECLARE_RUNTIME_FUNCTION(LoadPropertyWithInterceptorForLoad);
DECLARE_RUNTIME_FUNCTION(LoadPropertyWithInterceptorForCall);
DECLARE_RUNTIME_FUNCTION(StoreInterceptorProperty);
DECLARE_RUNTIME_FUNCTION(KeyedLoadPropertyWithInterceptor);


enum PrototypeCheckType { CHECK_ALL_MAPS, SKIP_RECEIVER };
enum IcCheckType { ELEMENT, PROPERTY };


// The stub compilers compile stubs for the stub cache.
class StubCompiler BASE_EMBEDDED {
 public:
  explicit StubCompiler(Isolate* isolate,
                        ExtraICState extra_ic_state = kNoExtraICState)
      : isolate_(isolate), extra_ic_state_(extra_ic_state),
        masm_(isolate, NULL, 256) { }

  Handle<Code> CompileLoadInitialize(Code::Flags flags);
  Handle<Code> CompileLoadPreMonomorphic(Code::Flags flags);
  Handle<Code> CompileLoadMegamorphic(Code::Flags flags);

  Handle<Code> CompileStoreInitialize(Code::Flags flags);
  Handle<Code> CompileStorePreMonomorphic(Code::Flags flags);
  Handle<Code> CompileStoreGeneric(Code::Flags flags);
  Handle<Code> CompileStoreMegamorphic(Code::Flags flags);

  // Static functions for generating parts of stubs.
  static void GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                  int index,
                                                  Register prototype);

  // Helper function used to check that the dictionary doesn't contain
  // the property. This function may return false negatives, so miss_label
  // must always call a backup property check that is complete.
  // This function is safe to call if the receiver has fast properties.
  // Name must be unique and receiver must be a heap object.
  static void GenerateDictionaryNegativeLookup(MacroAssembler* masm,
                                               Label* miss_label,
                                               Register receiver,
                                               Handle<Name> name,
                                               Register r0,
                                               Register r1);

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

  static void GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                            Register receiver,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss_label);

  // Generate code to check that a global property cell is empty. Create
  // the property cell at compilation time if no cell exists for the
  // property.
  static void GenerateCheckPropertyCell(MacroAssembler* masm,
                                        Handle<JSGlobalObject> global,
                                        Handle<Name> name,
                                        Register scratch,
                                        Label* miss);

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
  Register CheckPrototypes(Handle<HeapType> type,
                           Register object_reg,
                           Handle<JSObject> holder,
                           Register holder_reg,
                           Register scratch1,
                           Register scratch2,
                           Handle<Name> name,
                           Label* miss,
                           PrototypeCheckType check = CHECK_ALL_MAPS);

  static void GenerateFastApiCall(MacroAssembler* masm,
                                  const CallOptimization& optimization,
                                  Handle<Map> receiver_map,
                                  Register receiver,
                                  Register scratch,
                                  bool is_store,
                                  int argc,
                                  Register* values);

 protected:
  Handle<Code> GetCodeWithFlags(Code::Flags flags, const char* name);
  Handle<Code> GetCodeWithFlags(Code::Flags flags, Handle<Name> name);

  ExtraICState extra_state() { return extra_ic_state_; }

  MacroAssembler* masm() { return &masm_; }

  static void LookupPostInterceptor(Handle<JSObject> holder,
                                    Handle<Name> name,
                                    LookupResult* lookup);

  Isolate* isolate() { return isolate_; }
  Heap* heap() { return isolate()->heap(); }
  Factory* factory() { return isolate()->factory(); }

  static void GenerateTailCall(MacroAssembler* masm, Handle<Code> code);

 private:
  Isolate* isolate_;
  const ExtraICState extra_ic_state_;
  MacroAssembler masm_;
};


enum FrontendCheckType { PERFORM_INITIAL_CHECKS, SKIP_INITIAL_CHECKS };


class BaseLoadStoreStubCompiler: public StubCompiler {
 public:
  BaseLoadStoreStubCompiler(Isolate* isolate,
                            Code::Kind kind,
                            ExtraICState extra_ic_state = kNoExtraICState,
                            InlineCacheHolderFlag cache_holder = OWN_MAP)
      : StubCompiler(isolate, extra_ic_state),
        kind_(kind),
        cache_holder_(cache_holder) {
    InitializeRegisters();
  }
  virtual ~BaseLoadStoreStubCompiler() { }

  Handle<Code> CompileMonomorphicIC(Handle<HeapType> type,
                                    Handle<Code> handler,
                                    Handle<Name> name);

  Handle<Code> CompilePolymorphicIC(TypeHandleList* types,
                                    CodeHandleList* handlers,
                                    Handle<Name> name,
                                    Code::StubType type,
                                    IcCheckType check);

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

 protected:
  virtual Register HandlerFrontendHeader(Handle<HeapType> type,
                                         Register object_reg,
                                         Handle<JSObject> holder,
                                         Handle<Name> name,
                                         Label* miss) = 0;

  virtual void HandlerFrontendFooter(Handle<Name> name, Label* miss) = 0;

  Register HandlerFrontend(Handle<HeapType> type,
                           Register object_reg,
                           Handle<JSObject> holder,
                           Handle<Name> name);

  Handle<Code> GetCode(Code::Kind kind,
                       Code::StubType type,
                       Handle<Name> name);

  Handle<Code> GetICCode(Code::Kind kind,
                         Code::StubType type,
                         Handle<Name> name,
                         InlineCacheState state = MONOMORPHIC);
  Code::Kind kind() { return kind_; }

  Logger::LogEventsAndTags log_kind(Handle<Code> code) {
    if (!code->is_inline_cache_stub()) return Logger::STUB_TAG;
    if (kind_ == Code::LOAD_IC) {
      return code->ic_state() == MONOMORPHIC
          ? Logger::LOAD_IC_TAG : Logger::LOAD_POLYMORPHIC_IC_TAG;
    } else if (kind_ == Code::KEYED_LOAD_IC) {
      return code->ic_state() == MONOMORPHIC
          ? Logger::KEYED_LOAD_IC_TAG : Logger::KEYED_LOAD_POLYMORPHIC_IC_TAG;
    } else if (kind_ == Code::STORE_IC) {
      return code->ic_state() == MONOMORPHIC
          ? Logger::STORE_IC_TAG : Logger::STORE_POLYMORPHIC_IC_TAG;
    } else {
      return code->ic_state() == MONOMORPHIC
          ? Logger::KEYED_STORE_IC_TAG : Logger::KEYED_STORE_POLYMORPHIC_IC_TAG;
    }
  }
  void JitEvent(Handle<Name> name, Handle<Code> code);

  Register receiver() { return registers_[0]; }
  Register name()     { return registers_[1]; }
  Register scratch1() { return registers_[2]; }
  Register scratch2() { return registers_[3]; }
  Register scratch3() { return registers_[4]; }

  void InitializeRegisters();

  bool IncludesNumberType(TypeHandleList* types);

  Code::Kind kind_;
  InlineCacheHolderFlag cache_holder_;
  Register* registers_;
};


class LoadStubCompiler: public BaseLoadStoreStubCompiler {
 public:
  LoadStubCompiler(Isolate* isolate,
                   ExtraICState extra_ic_state = kNoExtraICState,
                   InlineCacheHolderFlag cache_holder = OWN_MAP,
                   Code::Kind kind = Code::LOAD_IC)
      : BaseLoadStoreStubCompiler(isolate, kind, extra_ic_state,
                                  cache_holder) { }
  virtual ~LoadStubCompiler() { }

  Handle<Code> CompileLoadField(Handle<HeapType> type,
                                Handle<JSObject> holder,
                                Handle<Name> name,
                                PropertyIndex index,
                                Representation representation);

  Handle<Code> CompileLoadCallback(Handle<HeapType> type,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   Handle<ExecutableAccessorInfo> callback);

  Handle<Code> CompileLoadCallback(Handle<HeapType> type,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   const CallOptimization& call_optimization);

  Handle<Code> CompileLoadConstant(Handle<HeapType> type,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   Handle<Object> value);

  Handle<Code> CompileLoadInterceptor(Handle<HeapType> type,
                                      Handle<JSObject> holder,
                                      Handle<Name> name);

  Handle<Code> CompileLoadViaGetter(Handle<HeapType> type,
                                    Handle<JSObject> holder,
                                    Handle<Name> name,
                                    Handle<JSFunction> getter);

  static void GenerateLoadViaGetter(MacroAssembler* masm,
                                    Handle<HeapType> type,
                                    Register receiver,
                                    Handle<JSFunction> getter);

  static void GenerateLoadViaGetterForDeopt(MacroAssembler* masm) {
    GenerateLoadViaGetter(
        masm, Handle<HeapType>::null(), no_reg, Handle<JSFunction>());
  }

  Handle<Code> CompileLoadNonexistent(Handle<HeapType> type,
                                      Handle<JSObject> last,
                                      Handle<Name> name);

  Handle<Code> CompileLoadGlobal(Handle<HeapType> type,
                                 Handle<GlobalObject> holder,
                                 Handle<PropertyCell> cell,
                                 Handle<Name> name,
                                 bool is_dont_delete);

 protected:
  ContextualMode contextual_mode() {
    return LoadIC::GetContextualMode(extra_state());
  }

  virtual Register HandlerFrontendHeader(Handle<HeapType> type,
                                         Register object_reg,
                                         Handle<JSObject> holder,
                                         Handle<Name> name,
                                         Label* miss);

  virtual void HandlerFrontendFooter(Handle<Name> name, Label* miss);

  Register CallbackHandlerFrontend(Handle<HeapType> type,
                                   Register object_reg,
                                   Handle<JSObject> holder,
                                   Handle<Name> name,
                                   Handle<Object> callback);
  void NonexistentHandlerFrontend(Handle<HeapType> type,
                                  Handle<JSObject> last,
                                  Handle<Name> name);

  void GenerateLoadField(Register reg,
                         Handle<JSObject> holder,
                         PropertyIndex field,
                         Representation representation);
  void GenerateLoadConstant(Handle<Object> value);
  void GenerateLoadCallback(Register reg,
                            Handle<ExecutableAccessorInfo> callback);
  void GenerateLoadCallback(const CallOptimization& call_optimization,
                            Handle<Map> receiver_map);
  void GenerateLoadInterceptor(Register holder_reg,
                               Handle<Object> object,
                               Handle<JSObject> holder,
                               LookupResult* lookup,
                               Handle<Name> name);
  void GenerateLoadPostInterceptor(Register reg,
                                   Handle<JSObject> interceptor_holder,
                                   Handle<Name> name,
                                   LookupResult* lookup);

 private:
  static Register* registers();
  Register scratch4() { return registers_[5]; }
  friend class BaseLoadStoreStubCompiler;
};


class KeyedLoadStubCompiler: public LoadStubCompiler {
 public:
  KeyedLoadStubCompiler(Isolate* isolate,
                        ExtraICState extra_ic_state = kNoExtraICState,
                        InlineCacheHolderFlag cache_holder = OWN_MAP)
      : LoadStubCompiler(isolate, extra_ic_state, cache_holder,
                         Code::KEYED_LOAD_IC) { }

  Handle<Code> CompileLoadElement(Handle<Map> receiver_map);

  void CompileElementHandlers(MapHandleList* receiver_maps,
                              CodeHandleList* handlers);

  static void GenerateLoadDictionaryElement(MacroAssembler* masm);

 private:
  static Register* registers();
  friend class BaseLoadStoreStubCompiler;
};


class StoreStubCompiler: public BaseLoadStoreStubCompiler {
 public:
  StoreStubCompiler(Isolate* isolate,
                    ExtraICState extra_ic_state,
                    Code::Kind kind = Code::STORE_IC)
      : BaseLoadStoreStubCompiler(isolate, kind, extra_ic_state) {}

  virtual ~StoreStubCompiler() { }

  Handle<Code> CompileStoreTransition(Handle<JSObject> object,
                                      LookupResult* lookup,
                                      Handle<Map> transition,
                                      Handle<Name> name);

  Handle<Code> CompileStoreField(Handle<JSObject> object,
                                 LookupResult* lookup,
                                 Handle<Name> name);

  Handle<Code> CompileStoreArrayLength(Handle<JSObject> object,
                                       LookupResult* lookup,
                                       Handle<Name> name);

  void GenerateStoreArrayLength();

  void GenerateNegativeHolderLookup(MacroAssembler* masm,
                                    Handle<JSObject> holder,
                                    Register holder_reg,
                                    Handle<Name> name,
                                    Label* miss);

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

  Handle<Code> CompileStoreCallback(Handle<JSObject> object,
                                    Handle<JSObject> holder,
                                    Handle<Name> name,
                                    Handle<ExecutableAccessorInfo> callback);

  Handle<Code> CompileStoreCallback(Handle<JSObject> object,
                                    Handle<JSObject> holder,
                                    Handle<Name> name,
                                    const CallOptimization& call_optimization);

  static void GenerateStoreViaSetter(MacroAssembler* masm,
                                     Handle<HeapType> type,
                                     Register receiver,
                                     Handle<JSFunction> setter);

  static void GenerateStoreViaSetterForDeopt(MacroAssembler* masm) {
    GenerateStoreViaSetter(
        masm, Handle<HeapType>::null(), no_reg, Handle<JSFunction>());
  }

  Handle<Code> CompileStoreViaSetter(Handle<JSObject> object,
                                     Handle<JSObject> holder,
                                     Handle<Name> name,
                                     Handle<JSFunction> setter);

  Handle<Code> CompileStoreInterceptor(Handle<JSObject> object,
                                       Handle<Name> name);

  static Builtins::Name SlowBuiltin(Code::Kind kind) {
    switch (kind) {
      case Code::STORE_IC: return Builtins::kStoreIC_Slow;
      case Code::KEYED_STORE_IC: return Builtins::kKeyedStoreIC_Slow;
      default: UNREACHABLE();
    }
    return Builtins::kStoreIC_Slow;
  }

 protected:
  virtual Register HandlerFrontendHeader(Handle<HeapType> type,
                                         Register object_reg,
                                         Handle<JSObject> holder,
                                         Handle<Name> name,
                                         Label* miss);

  virtual void HandlerFrontendFooter(Handle<Name> name, Label* miss);
  void GenerateRestoreName(MacroAssembler* masm,
                           Label* label,
                           Handle<Name> name);

 private:
  static Register* registers();
  static Register value();
  friend class BaseLoadStoreStubCompiler;
};


class KeyedStoreStubCompiler: public StoreStubCompiler {
 public:
  KeyedStoreStubCompiler(Isolate* isolate,
                         ExtraICState extra_ic_state)
      : StoreStubCompiler(isolate, extra_ic_state, Code::KEYED_STORE_IC) {}

  Handle<Code> CompileStoreElement(Handle<Map> receiver_map);

  Handle<Code> CompileStorePolymorphic(MapHandleList* receiver_maps,
                                       CodeHandleList* handler_stubs,
                                       MapHandleList* transitioned_maps);

  Handle<Code> CompileStoreElementPolymorphic(MapHandleList* receiver_maps);

  static void GenerateStoreDictionaryElement(MacroAssembler* masm);

 private:
  static Register* registers();

  KeyedAccessStoreMode store_mode() {
    return KeyedStoreIC::GetKeyedAccessStoreMode(extra_state());
  }

  Register transition_map() { return scratch1(); }

  friend class BaseLoadStoreStubCompiler;
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

  enum HolderLookup {
    kHolderNotFound,
    kHolderIsReceiver,
    kHolderFound
  };
  Handle<JSObject> LookupHolderOfExpectedType(
      Handle<Map> receiver_map,
      HolderLookup* holder_lookup) const;

  // Check if the api holder is between the receiver and the holder.
  bool IsCompatibleReceiver(Handle<Object> receiver,
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
