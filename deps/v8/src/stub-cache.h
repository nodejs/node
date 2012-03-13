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
    String* key;
    Code* value;
    Map* map;
  };

  void Initialize();


  // Computes the right stub matching. Inserts the result in the
  // cache before returning.  This might compile a stub if needed.
  Handle<Code> ComputeLoadNonexistent(Handle<String> name,
                                      Handle<JSObject> receiver);

  Handle<Code> ComputeLoadField(Handle<String> name,
                                Handle<JSObject> receiver,
                                Handle<JSObject> holder,
                                int field_index);

  Handle<Code> ComputeLoadCallback(Handle<String> name,
                                   Handle<JSObject> receiver,
                                   Handle<JSObject> holder,
                                   Handle<AccessorInfo> callback);

  Handle<Code> ComputeLoadConstant(Handle<String> name,
                                   Handle<JSObject> receiver,
                                   Handle<JSObject> holder,
                                   Handle<JSFunction> value);

  Handle<Code> ComputeLoadInterceptor(Handle<String> name,
                                      Handle<JSObject> receiver,
                                      Handle<JSObject> holder);

  Handle<Code> ComputeLoadNormal();

  Handle<Code> ComputeLoadGlobal(Handle<String> name,
                                 Handle<JSObject> receiver,
                                 Handle<GlobalObject> holder,
                                 Handle<JSGlobalPropertyCell> cell,
                                 bool is_dont_delete);

  // ---

  Handle<Code> ComputeKeyedLoadField(Handle<String> name,
                                     Handle<JSObject> receiver,
                                     Handle<JSObject> holder,
                                     int field_index);

  Handle<Code> ComputeKeyedLoadCallback(Handle<String> name,
                                        Handle<JSObject> receiver,
                                        Handle<JSObject> holder,
                                        Handle<AccessorInfo> callback);

  Handle<Code> ComputeKeyedLoadConstant(Handle<String> name,
                                        Handle<JSObject> receiver,
                                        Handle<JSObject> holder,
                                        Handle<JSFunction> value);

  Handle<Code> ComputeKeyedLoadInterceptor(Handle<String> name,
                                           Handle<JSObject> receiver,
                                           Handle<JSObject> holder);

  Handle<Code> ComputeKeyedLoadArrayLength(Handle<String> name,
                                           Handle<JSArray> receiver);

  Handle<Code> ComputeKeyedLoadStringLength(Handle<String> name,
                                            Handle<String> receiver);

  Handle<Code> ComputeKeyedLoadFunctionPrototype(Handle<String> name,
                                                 Handle<JSFunction> receiver);

  // ---

  Handle<Code> ComputeStoreField(Handle<String> name,
                                 Handle<JSObject> receiver,
                                 int field_index,
                                 Handle<Map> transition,
                                 StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreNormal(StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreGlobal(Handle<String> name,
                                  Handle<GlobalObject> receiver,
                                  Handle<JSGlobalPropertyCell> cell,
                                  StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreCallback(Handle<String> name,
                                    Handle<JSObject> receiver,
                                    Handle<AccessorInfo> callback,
                                    StrictModeFlag strict_mode);

  Handle<Code> ComputeStoreInterceptor(Handle<String> name,
                                       Handle<JSObject> receiver,
                                       StrictModeFlag strict_mode);

  // ---

  Handle<Code> ComputeKeyedStoreField(Handle<String> name,
                                      Handle<JSObject> receiver,
                                      int field_index,
                                      Handle<Map> transition,
                                      StrictModeFlag strict_mode);

  Handle<Code> ComputeKeyedLoadOrStoreElement(Handle<JSObject> receiver,
                                              KeyedIC::StubKind stub_kind,
                                              StrictModeFlag strict_mode);

  // ---

  Handle<Code> ComputeCallField(int argc,
                                Code::Kind,
                                Code::ExtraICState extra_state,
                                Handle<String> name,
                                Handle<Object> object,
                                Handle<JSObject> holder,
                                int index);

  Handle<Code> ComputeCallConstant(int argc,
                                   Code::Kind,
                                   Code::ExtraICState extra_state,
                                   Handle<String> name,
                                   Handle<Object> object,
                                   Handle<JSObject> holder,
                                   Handle<JSFunction> function);

  Handle<Code> ComputeCallInterceptor(int argc,
                                      Code::Kind,
                                      Code::ExtraICState extra_state,
                                      Handle<String> name,
                                      Handle<Object> object,
                                      Handle<JSObject> holder);

  Handle<Code> ComputeCallGlobal(int argc,
                                 Code::Kind,
                                 Code::ExtraICState extra_state,
                                 Handle<String> name,
                                 Handle<JSObject> receiver,
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

  Handle<Code> ComputeCallArguments(int argc, Code::Kind kind);

  Handle<Code> ComputeCallMegamorphic(int argc,
                                      Code::Kind kind,
                                      Code::ExtraICState state);

  Handle<Code> ComputeCallMiss(int argc,
                               Code::Kind kind,
                               Code::ExtraICState state);

  // Finds the Code object stored in the Heap::non_monomorphic_cache().
  Code* FindCallInitialize(int argc, RelocInfo::Mode mode, Code::Kind kind);

#ifdef ENABLE_DEBUGGER_SUPPORT
  Handle<Code> ComputeCallDebugBreak(int argc, Code::Kind kind);

  Handle<Code> ComputeCallDebugPrepareStepIn(int argc, Code::Kind kind);
#endif

  // Update cache for entry hash(name, map).
  Code* Set(String* name, Map* map, Code* code);

  // Clear the lookup table (@ mark compact collection).
  void Clear();

  // Collect all maps that match the name and flags.
  void CollectMatchingMaps(SmallMapList* types,
                           String* name,
                           Code::Flags flags,
                           Handle<Context> global_context);

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
  explicit StubCache(Isolate* isolate);

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
  static int PrimaryOffset(String* name, Code::Flags flags, Map* map) {
    // This works well because the heap object tag size and the hash
    // shift are equal.  Shifting down the length field to get the
    // hash code would effectively throw away two bits of the hash
    // code.
    STATIC_ASSERT(kHeapObjectTagSize == String::kHashShift);
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
  static int SecondaryOffset(String* name, Code::Flags flags, int seed) {
    // Use the seed from the primary cache in the secondary cache.
    uint32_t string_low32bits =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name));
    // We always set the in_loop bit to zero when generating the lookup code
    // so do it here too so the hash codes match.
    uint32_t iflags =
        (static_cast<uint32_t>(flags) & ~Code::kFlagsNotUsedInLookup);
    uint32_t key = (seed - string_low32bits) + iflags;
    return key & ((kSecondaryTableSize - 1) << kHeapObjectTagSize);
  }

  // Compute the entry for a given offset in exactly the same way as
  // we do in generated code.  We generate an hash code that already
  // ends in String::kHashShift 0s.  Then we multiply it so it is a multiple
  // of sizeof(Entry).  This makes it easier to avoid making mistakes
  // in the hashed offset computations.
  static Entry* entry(Entry* table, int offset) {
    const int multiplier = sizeof(*table) >> String::kHashShift;
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
DECLARE_RUNTIME_FUNCTION(MaybeObject*, LoadCallbackProperty);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, StoreCallbackProperty);


// Support functions for IC stubs for interceptors.
DECLARE_RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorOnly);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorForLoad);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorForCall);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, StoreInterceptorProperty);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, CallInterceptorProperty);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, KeyedLoadPropertyWithInterceptor);


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
                                       Handle<JSObject> holder,
                                       int index);

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

  static void GenerateStoreField(MacroAssembler* masm,
                                 Handle<JSObject> object,
                                 int index,
                                 Handle<Map> transition,
                                 Register receiver_reg,
                                 Register name_reg,
                                 Register scratch,
                                 Label* miss_label);

  static void GenerateLoadMiss(MacroAssembler* masm,
                               Code::Kind kind);

  static void GenerateKeyedLoadMissForceGeneric(MacroAssembler* masm);

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
                           Handle<String> name,
                           Label* miss) {
    return CheckPrototypes(object, object_reg, holder, holder_reg, scratch1,
                           scratch2, name, kInvalidProtoDepth, miss);
  }

  Register CheckPrototypes(Handle<JSObject> object,
                           Register object_reg,
                           Handle<JSObject> holder,
                           Register holder_reg,
                           Register scratch1,
                           Register scratch2,
                           Handle<String> name,
                           int save_at_depth,
                           Label* miss);

 protected:
  Handle<Code> GetCodeWithFlags(Code::Flags flags, const char* name);
  Handle<Code> GetCodeWithFlags(Code::Flags flags, Handle<String> name);

  MacroAssembler* masm() { return &masm_; }
  void set_failure(Failure* failure) { failure_ = failure; }

  void GenerateLoadField(Handle<JSObject> object,
                         Handle<JSObject> holder,
                         Register receiver,
                         Register scratch1,
                         Register scratch2,
                         Register scratch3,
                         int index,
                         Handle<String> name,
                         Label* miss);

  void GenerateLoadCallback(Handle<JSObject> object,
                            Handle<JSObject> holder,
                            Register receiver,
                            Register name_reg,
                            Register scratch1,
                            Register scratch2,
                            Register scratch3,
                            Handle<AccessorInfo> callback,
                            Handle<String> name,
                            Label* miss);

  void GenerateLoadConstant(Handle<JSObject> object,
                            Handle<JSObject> holder,
                            Register receiver,
                            Register scratch1,
                            Register scratch2,
                            Register scratch3,
                            Handle<JSFunction> value,
                            Handle<String> name,
                            Label* miss);

  void GenerateLoadInterceptor(Handle<JSObject> object,
                               Handle<JSObject> holder,
                               LookupResult* lookup,
                               Register receiver,
                               Register name_reg,
                               Register scratch1,
                               Register scratch2,
                               Register scratch3,
                               Handle<String> name,
                               Label* miss);

  static void LookupPostInterceptor(Handle<JSObject> holder,
                                    Handle<String> name,
                                    LookupResult* lookup);

  Isolate* isolate() { return isolate_; }
  Heap* heap() { return isolate()->heap(); }
  Factory* factory() { return isolate()->factory(); }

 private:
  Isolate* isolate_;
  MacroAssembler masm_;
  Failure* failure_;
};


class LoadStubCompiler: public StubCompiler {
 public:
  explicit LoadStubCompiler(Isolate* isolate) : StubCompiler(isolate) { }

  Handle<Code> CompileLoadNonexistent(Handle<String> name,
                                      Handle<JSObject> object,
                                      Handle<JSObject> last);

  Handle<Code> CompileLoadField(Handle<JSObject> object,
                                Handle<JSObject> holder,
                                int index,
                                Handle<String> name);

  Handle<Code> CompileLoadCallback(Handle<String> name,
                                   Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<AccessorInfo> callback);

  Handle<Code> CompileLoadConstant(Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<JSFunction> value,
                                   Handle<String> name);

  Handle<Code> CompileLoadInterceptor(Handle<JSObject> object,
                                      Handle<JSObject> holder,
                                      Handle<String> name);

  Handle<Code> CompileLoadGlobal(Handle<JSObject> object,
                                 Handle<GlobalObject> holder,
                                 Handle<JSGlobalPropertyCell> cell,
                                 Handle<String> name,
                                 bool is_dont_delete);

 private:
  Handle<Code> GetCode(PropertyType type, Handle<String> name);
};


class KeyedLoadStubCompiler: public StubCompiler {
 public:
  explicit KeyedLoadStubCompiler(Isolate* isolate) : StubCompiler(isolate) { }

  Handle<Code> CompileLoadField(Handle<String> name,
                                Handle<JSObject> object,
                                Handle<JSObject> holder,
                                int index);

  Handle<Code> CompileLoadCallback(Handle<String> name,
                                   Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<AccessorInfo> callback);

  Handle<Code> CompileLoadConstant(Handle<String> name,
                                   Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<JSFunction> value);

  Handle<Code> CompileLoadInterceptor(Handle<JSObject> object,
                                      Handle<JSObject> holder,
                                      Handle<String> name);

  Handle<Code> CompileLoadArrayLength(Handle<String> name);

  Handle<Code> CompileLoadStringLength(Handle<String> name);

  Handle<Code> CompileLoadFunctionPrototype(Handle<String> name);

  Handle<Code> CompileLoadElement(Handle<Map> receiver_map);

  Handle<Code> CompileLoadPolymorphic(MapHandleList* receiver_maps,
                                      CodeHandleList* handler_ics);

  static void GenerateLoadExternalArray(MacroAssembler* masm,
                                        ElementsKind elements_kind);

  static void GenerateLoadFastElement(MacroAssembler* masm);

  static void GenerateLoadFastDoubleElement(MacroAssembler* masm);

  static void GenerateLoadDictionaryElement(MacroAssembler* masm);

 private:
  Handle<Code> GetCode(PropertyType type,
                       Handle<String> name,
                       InlineCacheState state = MONOMORPHIC);
};


class StoreStubCompiler: public StubCompiler {
 public:
  StoreStubCompiler(Isolate* isolate, StrictModeFlag strict_mode)
    : StubCompiler(isolate), strict_mode_(strict_mode) { }


  Handle<Code> CompileStoreField(Handle<JSObject> object,
                                 int index,
                                 Handle<Map> transition,
                                 Handle<String> name);

  Handle<Code> CompileStoreCallback(Handle<JSObject> object,
                                    Handle<AccessorInfo> callback,
                                    Handle<String> name);

  Handle<Code> CompileStoreInterceptor(Handle<JSObject> object,
                                       Handle<String> name);

  Handle<Code> CompileStoreGlobal(Handle<GlobalObject> object,
                                  Handle<JSGlobalPropertyCell> holder,
                                  Handle<String> name);

 private:
  Handle<Code> GetCode(PropertyType type, Handle<String> name);

  StrictModeFlag strict_mode_;
};


class KeyedStoreStubCompiler: public StubCompiler {
 public:
  KeyedStoreStubCompiler(Isolate* isolate,
                         StrictModeFlag strict_mode,
                         KeyedAccessGrowMode grow_mode)
    : StubCompiler(isolate),
      strict_mode_(strict_mode),
      grow_mode_(grow_mode) { }

  Handle<Code> CompileStoreField(Handle<JSObject> object,
                                 int index,
                                 Handle<Map> transition,
                                 Handle<String> name);

  Handle<Code> CompileStoreElement(Handle<Map> receiver_map);

  Handle<Code> CompileStorePolymorphic(MapHandleList* receiver_maps,
                                       CodeHandleList* handler_stubs,
                                       MapHandleList* transitioned_maps);

  static void GenerateStoreFastElement(MacroAssembler* masm,
                                       bool is_js_array,
                                       ElementsKind element_kind,
                                       KeyedAccessGrowMode grow_mode);

  static void GenerateStoreFastDoubleElement(MacroAssembler* masm,
                                             bool is_js_array,
                                             KeyedAccessGrowMode grow_mode);

  static void GenerateStoreExternalArray(MacroAssembler* masm,
                                         ElementsKind elements_kind);

  static void GenerateStoreDictionaryElement(MacroAssembler* masm);

 private:
  Handle<Code> GetCode(PropertyType type,
                       Handle<String> name,
                       InlineCacheState state = MONOMORPHIC);

  StrictModeFlag strict_mode_;
  KeyedAccessGrowMode grow_mode_;
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
                                int index,
                                Handle<String> name);

  Handle<Code> CompileCallConstant(Handle<Object> object,
                                   Handle<JSObject> holder,
                                   Handle<JSFunction> function,
                                   Handle<String> name,
                                   CheckType check);

  Handle<Code> CompileCallInterceptor(Handle<JSObject> object,
                                      Handle<JSObject> holder,
                                      Handle<String> name);

  Handle<Code> CompileCallGlobal(Handle<JSObject> object,
                                 Handle<GlobalObject> holder,
                                 Handle<JSGlobalPropertyCell> cell,
                                 Handle<JSFunction> function,
                                 Handle<String> name);

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

  Handle<Code> GetCode(PropertyType type, Handle<String> name);
  Handle<Code> GetCode(Handle<JSFunction> function);

  const ParameterCount& arguments() { return arguments_; }

  void GenerateNameCheck(Handle<String> name, Label* miss);

  void GenerateGlobalReceiverCheck(Handle<JSObject> object,
                                   Handle<JSObject> holder,
                                   Handle<String> name,
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
