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

#ifndef V8_STUB_CACHE_H_
#define V8_STUB_CACHE_H_

#include "allocation.h"
#include "arguments.h"
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
  };

  void Initialize(bool create_heap_objects);


  // Computes the right stub matching. Inserts the result in the
  // cache before returning.  This might compile a stub if needed.
  MUST_USE_RESULT MaybeObject* ComputeLoadNonexistent(
      String* name,
      JSObject* receiver);

  MUST_USE_RESULT MaybeObject* ComputeLoadField(String* name,
                                                JSObject* receiver,
                                                JSObject* holder,
                                                int field_index);

  MUST_USE_RESULT MaybeObject* ComputeLoadCallback(
      String* name,
      JSObject* receiver,
      JSObject* holder,
      AccessorInfo* callback);

  MUST_USE_RESULT MaybeObject* ComputeLoadConstant(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   Object* value);

  MUST_USE_RESULT MaybeObject* ComputeLoadInterceptor(
      String* name,
      JSObject* receiver,
      JSObject* holder);

  MUST_USE_RESULT MaybeObject* ComputeLoadNormal();


  MUST_USE_RESULT MaybeObject* ComputeLoadGlobal(
      String* name,
      JSObject* receiver,
      GlobalObject* holder,
      JSGlobalPropertyCell* cell,
      bool is_dont_delete);


  // ---

  MUST_USE_RESULT MaybeObject* ComputeKeyedLoadField(String* name,
                                                     JSObject* receiver,
                                                     JSObject* holder,
                                                     int field_index);

  MUST_USE_RESULT MaybeObject* ComputeKeyedLoadCallback(
      String* name,
      JSObject* receiver,
      JSObject* holder,
      AccessorInfo* callback);

  MUST_USE_RESULT MaybeObject* ComputeKeyedLoadConstant(
      String* name,
      JSObject* receiver,
      JSObject* holder,
      Object* value);

  MUST_USE_RESULT MaybeObject* ComputeKeyedLoadInterceptor(
      String* name,
      JSObject* receiver,
      JSObject* holder);

  MUST_USE_RESULT MaybeObject* ComputeKeyedLoadArrayLength(
      String* name,
      JSArray* receiver);

  MUST_USE_RESULT MaybeObject* ComputeKeyedLoadStringLength(
      String* name,
      String* receiver);

  MUST_USE_RESULT MaybeObject* ComputeKeyedLoadFunctionPrototype(
      String* name,
      JSFunction* receiver);

  // ---

  MUST_USE_RESULT MaybeObject* ComputeStoreField(
      String* name,
      JSObject* receiver,
      int field_index,
      Map* transition,
      StrictModeFlag strict_mode);

  MUST_USE_RESULT MaybeObject* ComputeStoreNormal(
      StrictModeFlag strict_mode);

  MUST_USE_RESULT MaybeObject* ComputeStoreGlobal(
      String* name,
      GlobalObject* receiver,
      JSGlobalPropertyCell* cell,
      StrictModeFlag strict_mode);

  MUST_USE_RESULT MaybeObject* ComputeStoreCallback(
      String* name,
      JSObject* receiver,
      AccessorInfo* callback,
      StrictModeFlag strict_mode);

  MUST_USE_RESULT MaybeObject* ComputeStoreInterceptor(
      String* name,
      JSObject* receiver,
      StrictModeFlag strict_mode);

  // ---

  MUST_USE_RESULT MaybeObject* ComputeKeyedStoreField(
      String* name,
      JSObject* receiver,
      int field_index,
      Map* transition,
      StrictModeFlag strict_mode);

  MUST_USE_RESULT MaybeObject* ComputeKeyedLoadOrStoreElement(
      JSObject* receiver,
      bool is_store,
      StrictModeFlag strict_mode);

  // ---

  MUST_USE_RESULT MaybeObject* ComputeCallField(
      int argc,
      Code::Kind,
      Code::ExtraICState extra_ic_state,
      String* name,
      Object* object,
      JSObject* holder,
      int index);

  MUST_USE_RESULT MaybeObject* ComputeCallConstant(
      int argc,
      Code::Kind,
      Code::ExtraICState extra_ic_state,
      String* name,
      Object* object,
      JSObject* holder,
      JSFunction* function);

  MUST_USE_RESULT MaybeObject* ComputeCallNormal(
      int argc,
      Code::Kind,
      Code::ExtraICState extra_ic_state,
      String* name,
      JSObject* receiver);

  MUST_USE_RESULT MaybeObject* ComputeCallInterceptor(
      int argc,
      Code::Kind,
      Code::ExtraICState extra_ic_state,
      String* name,
      Object* object,
      JSObject* holder);

  MUST_USE_RESULT MaybeObject* ComputeCallGlobal(
      int argc,
      Code::Kind,
      Code::ExtraICState extra_ic_state,
      String* name,
      JSObject* receiver,
      GlobalObject* holder,
      JSGlobalPropertyCell* cell,
      JSFunction* function);

  // ---

  MUST_USE_RESULT MaybeObject* ComputeCallInitialize(int argc,
                                                     RelocInfo::Mode mode,
                                                     Code::Kind kind);

  Handle<Code> ComputeCallInitialize(int argc,
                                     RelocInfo::Mode mode);

  Handle<Code> ComputeKeyedCallInitialize(int argc);

  MUST_USE_RESULT MaybeObject* ComputeCallPreMonomorphic(
      int argc,
      Code::Kind kind,
      Code::ExtraICState extra_ic_state);

  MUST_USE_RESULT MaybeObject* ComputeCallNormal(int argc,
                                                 Code::Kind kind,
                                                 Code::ExtraICState state);

  MUST_USE_RESULT MaybeObject* ComputeCallArguments(int argc,
                                                    Code::Kind kind);

  MUST_USE_RESULT MaybeObject* ComputeCallMegamorphic(int argc,
                                                      Code::Kind kind,
                                                      Code::ExtraICState state);

  MUST_USE_RESULT MaybeObject* ComputeCallMiss(int argc,
                                               Code::Kind kind,
                                               Code::ExtraICState state);

  // Finds the Code object stored in the Heap::non_monomorphic_cache().
  MUST_USE_RESULT Code* FindCallInitialize(int argc,
                                           RelocInfo::Mode mode,
                                           Code::Kind kind);

#ifdef ENABLE_DEBUGGER_SUPPORT
  MUST_USE_RESULT MaybeObject* ComputeCallDebugBreak(int argc, Code::Kind kind);

  MUST_USE_RESULT MaybeObject* ComputeCallDebugPrepareStepIn(int argc,
                                                             Code::Kind kind);
#endif

  // Update cache for entry hash(name, map).
  Code* Set(String* name, Map* map, Code* code);

  // Clear the lookup table (@ mark compact collection).
  void Clear();

  // Collect all maps that match the name and flags.
  void CollectMatchingMaps(SmallMapList* types,
                           String* name,
                           Code::Flags flags);

  // Generate code for probing the stub cache table.
  // Arguments extra and extra2 may be used to pass additional scratch
  // registers. Set to no_reg if not needed.
  void GenerateProbe(MacroAssembler* masm,
                     Code::Flags flags,
                     Register receiver,
                     Register name,
                     Register scratch,
                     Register extra,
                     Register extra2 = no_reg);

  enum Table {
    kPrimary,
    kSecondary
  };


  SCTableReference key_reference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->key));
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

 private:
  explicit StubCache(Isolate* isolate);

  friend class Isolate;
  friend class SCTableReference;
  static const int kPrimaryTableSize = 2048;
  static const int kSecondaryTableSize = 512;
  Entry primary_[kPrimaryTableSize];
  Entry secondary_[kSecondaryTableSize];

  // Computes the hashed offsets for primary and secondary caches.
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

  static int SecondaryOffset(String* name, Code::Flags flags, int seed) {
    // Use the seed from the primary cache in the secondary cache.
    uint32_t string_low32bits =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name));
    uint32_t key = seed - string_low32bits + flags;
    return key & ((kSecondaryTableSize - 1) << kHeapObjectTagSize);
  }

  // Compute the entry for a given offset in exactly the same way as
  // we do in generated code.  We generate an hash code that already
  // ends in String::kHashShift 0s.  Then we shift it so it is a multiple
  // of sizeof(Entry).  This makes it easier to avoid making mistakes
  // in the hashed offset computations.
  static Entry* entry(Entry* table, int offset) {
    const int shift_amount = kPointerSizeLog2 + 1 - String::kHashShift;
    return reinterpret_cast<Entry*>(
        reinterpret_cast<Address>(table) + (offset << shift_amount));
  }

  Isolate* isolate_;

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


// The stub compiler compiles stubs for the stub cache.
class StubCompiler BASE_EMBEDDED {
 public:
  StubCompiler()
      : scope_(), masm_(Isolate::Current(), NULL, 256), failure_(NULL) { }

  MUST_USE_RESULT MaybeObject* CompileCallInitialize(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallPreMonomorphic(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallNormal(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallMegamorphic(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallArguments(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallMiss(Code::Flags flags);
#ifdef ENABLE_DEBUGGER_SUPPORT
  MUST_USE_RESULT MaybeObject* CompileCallDebugBreak(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallDebugPrepareStepIn(Code::Flags flags);
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
                                       Register dst, Register src,
                                       JSObject* holder, int index);

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
                                 JSObject* object,
                                 int index,
                                 Map* transition,
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

  Register CheckPrototypes(JSObject* object,
                           Register object_reg,
                           JSObject* holder,
                           Register holder_reg,
                           Register scratch1,
                           Register scratch2,
                           String* name,
                           Label* miss) {
    return CheckPrototypes(object, object_reg, holder, holder_reg, scratch1,
                           scratch2, name, kInvalidProtoDepth, miss);
  }

  Register CheckPrototypes(JSObject* object,
                           Register object_reg,
                           JSObject* holder,
                           Register holder_reg,
                           Register scratch1,
                           Register scratch2,
                           String* name,
                           int save_at_depth,
                           Label* miss);

 protected:
  MaybeObject* GetCodeWithFlags(Code::Flags flags, const char* name);
  MaybeObject* GetCodeWithFlags(Code::Flags flags, String* name);

  MacroAssembler* masm() { return &masm_; }
  void set_failure(Failure* failure) { failure_ = failure; }

  void GenerateLoadField(JSObject* object,
                         JSObject* holder,
                         Register receiver,
                         Register scratch1,
                         Register scratch2,
                         Register scratch3,
                         int index,
                         String* name,
                         Label* miss);

  MaybeObject* GenerateLoadCallback(JSObject* object,
                                    JSObject* holder,
                                    Register receiver,
                                    Register name_reg,
                                    Register scratch1,
                                    Register scratch2,
                                    Register scratch3,
                                    AccessorInfo* callback,
                                    String* name,
                                    Label* miss);

  void GenerateLoadConstant(JSObject* object,
                            JSObject* holder,
                            Register receiver,
                            Register scratch1,
                            Register scratch2,
                            Register scratch3,
                            Object* value,
                            String* name,
                            Label* miss);

  void GenerateLoadInterceptor(JSObject* object,
                               JSObject* holder,
                               LookupResult* lookup,
                               Register receiver,
                               Register name_reg,
                               Register scratch1,
                               Register scratch2,
                               Register scratch3,
                               String* name,
                               Label* miss);

  static void LookupPostInterceptor(JSObject* holder,
                                    String* name,
                                    LookupResult* lookup);

  Isolate* isolate() { return scope_.isolate(); }
  Heap* heap() { return isolate()->heap(); }
  Factory* factory() { return isolate()->factory(); }

 private:
  HandleScope scope_;
  MacroAssembler masm_;
  Failure* failure_;
};


class LoadStubCompiler: public StubCompiler {
 public:
  MUST_USE_RESULT MaybeObject* CompileLoadNonexistent(String* name,
                                                      JSObject* object,
                                                      JSObject* last);

  MUST_USE_RESULT MaybeObject* CompileLoadField(JSObject* object,
                                                JSObject* holder,
                                                int index,
                                                String* name);

  MUST_USE_RESULT MaybeObject* CompileLoadCallback(String* name,
                                                   JSObject* object,
                                                   JSObject* holder,
                                                   AccessorInfo* callback);

  MUST_USE_RESULT MaybeObject* CompileLoadConstant(JSObject* object,
                                                   JSObject* holder,
                                                   Object* value,
                                                   String* name);

  MUST_USE_RESULT MaybeObject* CompileLoadInterceptor(JSObject* object,
                                                      JSObject* holder,
                                                      String* name);

  MUST_USE_RESULT MaybeObject* CompileLoadGlobal(JSObject* object,
                                                 GlobalObject* holder,
                                                 JSGlobalPropertyCell* cell,
                                                 String* name,
                                                 bool is_dont_delete);

 private:
  MUST_USE_RESULT MaybeObject* GetCode(PropertyType type, String* name);
};


class KeyedLoadStubCompiler: public StubCompiler {
 public:
  MUST_USE_RESULT MaybeObject* CompileLoadField(String* name,
                                                JSObject* object,
                                                JSObject* holder,
                                                int index);

  MUST_USE_RESULT MaybeObject* CompileLoadCallback(String* name,
                                                   JSObject* object,
                                                   JSObject* holder,
                                                   AccessorInfo* callback);

  MUST_USE_RESULT MaybeObject* CompileLoadConstant(String* name,
                                                   JSObject* object,
                                                   JSObject* holder,
                                                   Object* value);

  MUST_USE_RESULT MaybeObject* CompileLoadInterceptor(JSObject* object,
                                                      JSObject* holder,
                                                      String* name);

  MUST_USE_RESULT MaybeObject* CompileLoadArrayLength(String* name);
  MUST_USE_RESULT MaybeObject* CompileLoadStringLength(String* name);
  MUST_USE_RESULT MaybeObject* CompileLoadFunctionPrototype(String* name);

  MUST_USE_RESULT MaybeObject* CompileLoadElement(Map* receiver_map);

  MUST_USE_RESULT MaybeObject* CompileLoadMegamorphic(
      MapList* receiver_maps,
      CodeList* handler_ics);

  static void GenerateLoadExternalArray(MacroAssembler* masm,
                                        ElementsKind elements_kind);

  static void GenerateLoadFastElement(MacroAssembler* masm);

  static void GenerateLoadFastDoubleElement(MacroAssembler* masm);

  static void GenerateLoadDictionaryElement(MacroAssembler* masm);

 private:
  MaybeObject* GetCode(PropertyType type,
                       String* name,
                       InlineCacheState state = MONOMORPHIC);
};


class StoreStubCompiler: public StubCompiler {
 public:
  explicit StoreStubCompiler(StrictModeFlag strict_mode)
    : strict_mode_(strict_mode) { }

  MUST_USE_RESULT MaybeObject* CompileStoreField(JSObject* object,
                                                 int index,
                                                 Map* transition,
                                                 String* name);

  MUST_USE_RESULT MaybeObject* CompileStoreCallback(JSObject* object,
                                                    AccessorInfo* callbacks,
                                                    String* name);
  MUST_USE_RESULT MaybeObject* CompileStoreInterceptor(JSObject* object,
                                                       String* name);
  MUST_USE_RESULT MaybeObject* CompileStoreGlobal(GlobalObject* object,
                                                  JSGlobalPropertyCell* holder,
                                                  String* name);


 private:
  MaybeObject* GetCode(PropertyType type, String* name);

  StrictModeFlag strict_mode_;
};


class KeyedStoreStubCompiler: public StubCompiler {
 public:
  explicit KeyedStoreStubCompiler(StrictModeFlag strict_mode)
    : strict_mode_(strict_mode) { }

  MUST_USE_RESULT MaybeObject* CompileStoreField(JSObject* object,
                                                 int index,
                                                 Map* transition,
                                                 String* name);

  MUST_USE_RESULT MaybeObject* CompileStoreElement(Map* receiver_map);

  MUST_USE_RESULT MaybeObject* CompileStoreMegamorphic(
      MapList* receiver_maps,
      CodeList* handler_ics);

  static void GenerateStoreFastElement(MacroAssembler* masm,
                                       bool is_js_array);

  static void GenerateStoreFastDoubleElement(MacroAssembler* masm,
                                             bool is_js_array);

  static void GenerateStoreExternalArray(MacroAssembler* masm,
                                         ElementsKind elements_kind);

  static void GenerateStoreDictionaryElement(MacroAssembler* masm);

 private:
  MaybeObject* GetCode(PropertyType type,
                       String* name,
                       InlineCacheState state = MONOMORPHIC);

  StrictModeFlag strict_mode_;
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
  CallStubCompiler(int argc,
                   Code::Kind kind,
                   Code::ExtraICState extra_ic_state,
                   InlineCacheHolderFlag cache_holder);

  MUST_USE_RESULT MaybeObject* CompileCallField(
      JSObject* object,
      JSObject* holder,
      int index,
      String* name);

  MUST_USE_RESULT MaybeObject* CompileCallConstant(
      Object* object,
      JSObject* holder,
      JSFunction* function,
      String* name,
      CheckType check);

  MUST_USE_RESULT MaybeObject* CompileCallInterceptor(
      JSObject* object,
      JSObject* holder,
      String* name);

  MUST_USE_RESULT MaybeObject* CompileCallGlobal(
      JSObject* object,
      GlobalObject* holder,
      JSGlobalPropertyCell* cell,
      JSFunction* function,
      String* name);

  static bool HasCustomCallGenerator(JSFunction* function);

 private:
  // Compiles a custom call constant/global IC. For constant calls
  // cell is NULL. Returns undefined if there is no custom call code
  // for the given function or it can't be generated.
  MUST_USE_RESULT MaybeObject* CompileCustomCall(Object* object,
                                                 JSObject* holder,
                                                 JSGlobalPropertyCell* cell,
                                                 JSFunction* function,
                                                 String* name);

#define DECLARE_CALL_GENERATOR(name)                                           \
  MUST_USE_RESULT MaybeObject* Compile##name##Call(Object* object,             \
                                                   JSObject* holder,           \
                                                   JSGlobalPropertyCell* cell, \
                                                   JSFunction* function,       \
                                                   String* fname);
  CUSTOM_CALL_IC_GENERATORS(DECLARE_CALL_GENERATOR)
#undef DECLARE_CALL_GENERATOR

  MUST_USE_RESULT MaybeObject* CompileFastApiCall(
      const CallOptimization& optimization,
      Object* object,
      JSObject* holder,
      JSGlobalPropertyCell* cell,
      JSFunction* function,
      String* name);

  const ParameterCount arguments_;
  const Code::Kind kind_;
  const Code::ExtraICState extra_ic_state_;
  const InlineCacheHolderFlag cache_holder_;

  const ParameterCount& arguments() { return arguments_; }

  MUST_USE_RESULT MaybeObject* GetCode(PropertyType type, String* name);

  // Convenience function. Calls GetCode above passing
  // CONSTANT_FUNCTION type and the name of the given function.
  MUST_USE_RESULT MaybeObject* GetCode(JSFunction* function);

  void GenerateNameCheck(String* name, Label* miss);

  void GenerateGlobalReceiverCheck(JSObject* object,
                                   JSObject* holder,
                                   String* name,
                                   Label* miss);

  // Generates code to load the function from the cell checking that
  // it still contains the same function.
  void GenerateLoadFunctionFromCell(JSGlobalPropertyCell* cell,
                                    JSFunction* function,
                                    Label* miss);

  // Generates a jump to CallIC miss stub. Returns Failure if the jump cannot
  // be generated.
  MUST_USE_RESULT MaybeObject* GenerateMissBranch();
};


class ConstructStubCompiler: public StubCompiler {
 public:
  explicit ConstructStubCompiler() {}

  MUST_USE_RESULT MaybeObject* CompileConstructStub(JSFunction* function);

 private:
  MaybeObject* GetCode();
};


// Holds information about possible function call optimizations.
class CallOptimization BASE_EMBEDDED {
 public:
  explicit CallOptimization(LookupResult* lookup);

  explicit CallOptimization(JSFunction* function);

  bool is_constant_call() const {
    return constant_function_ != NULL;
  }

  JSFunction* constant_function() const {
    ASSERT(constant_function_ != NULL);
    return constant_function_;
  }

  bool is_simple_api_call() const {
    return is_simple_api_call_;
  }

  FunctionTemplateInfo* expected_receiver_type() const {
    ASSERT(is_simple_api_call_);
    return expected_receiver_type_;
  }

  CallHandlerInfo* api_call_info() const {
    ASSERT(is_simple_api_call_);
    return api_call_info_;
  }

  // Returns the depth of the object having the expected type in the
  // prototype chain between the two arguments.
  int GetPrototypeDepthOfExpectedType(JSObject* object,
                                      JSObject* holder) const;

 private:
  void Initialize(JSFunction* function);

  // Determines whether the given function can be called using the
  // fast api call builtin.
  void AnalyzePossibleApiFunction(JSFunction* function);

  JSFunction* constant_function_;
  bool is_simple_api_call_;
  FunctionTemplateInfo* expected_receiver_type_;
  CallHandlerInfo* api_call_info_;
};


} }  // namespace v8::internal

#endif  // V8_STUB_CACHE_H_
