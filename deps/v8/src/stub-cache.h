// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "macro-assembler.h"

namespace v8 {
namespace internal {


// The stub cache is used for megamorphic calls and property accesses.
// It maps (map, name, type)->Code*

// The design of the table uses the inline cache stubs used for
// mono-morphic calls. The beauty of this, we do not have to
// invalidate the cache whenever a prototype map is changed.  The stub
// validates the map chain as in the mono-morphic case.

class SCTableReference;

class StubCache : public AllStatic {
 public:
  struct Entry {
    String* key;
    Code* value;
  };


  static void Initialize(bool create_heap_objects);

  // Computes the right stub matching. Inserts the result in the
  // cache before returning.  This might compile a stub if needed.
  MUST_USE_RESULT static MaybeObject* ComputeLoadNonexistent(
      String* name,
      JSObject* receiver);

  MUST_USE_RESULT static MaybeObject* ComputeLoadField(String* name,
                                                       JSObject* receiver,
                                                       JSObject* holder,
                                                       int field_index);

  MUST_USE_RESULT static MaybeObject* ComputeLoadCallback(
      String* name,
      JSObject* receiver,
      JSObject* holder,
      AccessorInfo* callback);

  MUST_USE_RESULT static MaybeObject* ComputeLoadConstant(String* name,
                                                          JSObject* receiver,
                                                          JSObject* holder,
                                                          Object* value);

  MUST_USE_RESULT static MaybeObject* ComputeLoadInterceptor(String* name,
                                                             JSObject* receiver,
                                                             JSObject* holder);

  MUST_USE_RESULT static MaybeObject* ComputeLoadNormal();


  MUST_USE_RESULT static MaybeObject* ComputeLoadGlobal(
      String* name,
      JSObject* receiver,
      GlobalObject* holder,
      JSGlobalPropertyCell* cell,
      bool is_dont_delete);


  // ---

  MUST_USE_RESULT static MaybeObject* ComputeKeyedLoadField(String* name,
                                                            JSObject* receiver,
                                                            JSObject* holder,
                                                            int field_index);

  MUST_USE_RESULT static MaybeObject* ComputeKeyedLoadCallback(
      String* name,
      JSObject* receiver,
      JSObject* holder,
      AccessorInfo* callback);

  MUST_USE_RESULT static MaybeObject* ComputeKeyedLoadConstant(
      String* name,
      JSObject* receiver,
      JSObject* holder,
      Object* value);

  MUST_USE_RESULT static MaybeObject* ComputeKeyedLoadInterceptor(
      String* name,
      JSObject* receiver,
      JSObject* holder);

  MUST_USE_RESULT static MaybeObject* ComputeKeyedLoadArrayLength(
      String* name,
      JSArray* receiver);

  MUST_USE_RESULT static MaybeObject* ComputeKeyedLoadStringLength(
      String* name,
      String* receiver);

  MUST_USE_RESULT static MaybeObject* ComputeKeyedLoadFunctionPrototype(
      String* name,
      JSFunction* receiver);

  // ---

  MUST_USE_RESULT static MaybeObject* ComputeStoreField(String* name,
                                                        JSObject* receiver,
                                                        int field_index,
                                                        Map* transition = NULL);

  MUST_USE_RESULT static MaybeObject* ComputeStoreNormal();

  MUST_USE_RESULT static MaybeObject* ComputeStoreGlobal(
      String* name,
      GlobalObject* receiver,
      JSGlobalPropertyCell* cell);

  MUST_USE_RESULT static MaybeObject* ComputeStoreCallback(
      String* name,
      JSObject* receiver,
      AccessorInfo* callback);

  MUST_USE_RESULT static MaybeObject* ComputeStoreInterceptor(
      String* name,
      JSObject* receiver);

  // ---

  MUST_USE_RESULT static MaybeObject* ComputeKeyedStoreField(
      String* name,
      JSObject* receiver,
      int field_index,
      Map* transition = NULL);

  // ---

  MUST_USE_RESULT static MaybeObject* ComputeCallField(int argc,
                                                       InLoopFlag in_loop,
                                                       Code::Kind,
                                                       String* name,
                                                       Object* object,
                                                       JSObject* holder,
                                                       int index);

  MUST_USE_RESULT static MaybeObject* ComputeCallConstant(int argc,
                                                          InLoopFlag in_loop,
                                                          Code::Kind,
                                                          String* name,
                                                          Object* object,
                                                          JSObject* holder,
                                                          JSFunction* function);

  MUST_USE_RESULT static MaybeObject* ComputeCallNormal(int argc,
                                                        InLoopFlag in_loop,
                                                        Code::Kind,
                                                        String* name,
                                                        JSObject* receiver);

  MUST_USE_RESULT static MaybeObject* ComputeCallInterceptor(int argc,
                                                             Code::Kind,
                                                             String* name,
                                                             Object* object,
                                                             JSObject* holder);

  MUST_USE_RESULT static MaybeObject* ComputeCallGlobal(
      int argc,
      InLoopFlag in_loop,
      Code::Kind,
      String* name,
      JSObject* receiver,
      GlobalObject* holder,
      JSGlobalPropertyCell* cell,
      JSFunction* function);

  // ---

  MUST_USE_RESULT static MaybeObject* ComputeCallInitialize(int argc,
                                                            InLoopFlag in_loop,
                                                            Code::Kind kind);

  MUST_USE_RESULT static MaybeObject* ComputeCallPreMonomorphic(
      int argc,
      InLoopFlag in_loop,
      Code::Kind kind);

  MUST_USE_RESULT static MaybeObject* ComputeCallNormal(int argc,
                                                        InLoopFlag in_loop,
                                                        Code::Kind kind);

  MUST_USE_RESULT static MaybeObject* ComputeCallMegamorphic(int argc,
                                                             InLoopFlag in_loop,
                                                             Code::Kind kind);

  MUST_USE_RESULT static MaybeObject* ComputeCallMiss(int argc,
                                                      Code::Kind kind);

  // Finds the Code object stored in the Heap::non_monomorphic_cache().
  MUST_USE_RESULT static Code* FindCallInitialize(int argc,
                                                  InLoopFlag in_loop,
                                                  Code::Kind kind);

#ifdef ENABLE_DEBUGGER_SUPPORT
  MUST_USE_RESULT static MaybeObject* ComputeCallDebugBreak(int argc,
                                                            Code::Kind kind);

  MUST_USE_RESULT static MaybeObject* ComputeCallDebugPrepareStepIn(
      int argc,
      Code::Kind kind);
#endif

  // Update cache for entry hash(name, map).
  static Code* Set(String* name, Map* map, Code* code);

  // Clear the lookup table (@ mark compact collection).
  static void Clear();

  // Generate code for probing the stub cache table.
  // Arguments extra and extra2 may be used to pass additional scratch
  // registers. Set to no_reg if not needed.
  static void GenerateProbe(MacroAssembler* masm,
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

 private:
  friend class SCTableReference;
  static const int kPrimaryTableSize = 2048;
  static const int kSecondaryTableSize = 512;
  static Entry primary_[];
  static Entry secondary_[];

  // Computes the hashed offsets for primary and secondary caches.
  static int PrimaryOffset(String* name, Code::Flags flags, Map* map) {
    // This works well because the heap object tag size and the hash
    // shift are equal.  Shifting down the length field to get the
    // hash code would effectively throw away two bits of the hash
    // code.
    ASSERT(kHeapObjectTagSize == String::kHashShift);
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
    // We always set the in_loop bit to zero when generating the lookup code
    // so do it here too so the hash codes match.
    uint32_t iflags =
        (static_cast<uint32_t>(flags) & ~Code::kFlagsICInLoopMask);
    uint32_t key = seed - string_low32bits + iflags;
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
};


class SCTableReference {
 public:
  static SCTableReference keyReference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->key));
  }


  static SCTableReference valueReference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->value));
  }

  Address address() const { return address_; }

 private:
  explicit SCTableReference(Address address) : address_(address) {}

  static StubCache::Entry* first_entry(StubCache::Table table) {
    switch (table) {
      case StubCache::kPrimary: return StubCache::primary_;
      case StubCache::kSecondary: return StubCache::secondary_;
    }
    UNREACHABLE();
    return NULL;
  }

  Address address_;
};

// ------------------------------------------------------------------------


// Support functions for IC stubs for callbacks.
MaybeObject* LoadCallbackProperty(Arguments args);
MaybeObject* StoreCallbackProperty(Arguments args);


// Support functions for IC stubs for interceptors.
MaybeObject* LoadPropertyWithInterceptorOnly(Arguments args);
MaybeObject* LoadPropertyWithInterceptorForLoad(Arguments args);
MaybeObject* LoadPropertyWithInterceptorForCall(Arguments args);
MaybeObject* StoreInterceptorProperty(Arguments args);
MaybeObject* CallInterceptorProperty(Arguments args);
MaybeObject* KeyedLoadPropertyWithInterceptor(Arguments args);


// The stub compiler compiles stubs for the stub cache.
class StubCompiler BASE_EMBEDDED {
 public:
  enum CheckType {
    RECEIVER_MAP_CHECK,
    STRING_CHECK,
    NUMBER_CHECK,
    BOOLEAN_CHECK
  };

  StubCompiler() : scope_(), masm_(NULL, 256), failure_(NULL) { }

  MUST_USE_RESULT MaybeObject* CompileCallInitialize(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallPreMonomorphic(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallNormal(Code::Flags flags);
  MUST_USE_RESULT MaybeObject* CompileCallMegamorphic(Code::Flags flags);
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
                                       Label* miss_label);

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

  static void GenerateLoadMiss(MacroAssembler* masm, Code::Kind kind);

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

  bool GenerateLoadCallback(JSObject* object,
                            JSObject* holder,
                            Register receiver,
                            Register name_reg,
                            Register scratch1,
                            Register scratch2,
                            Register scratch3,
                            AccessorInfo* callback,
                            String* name,
                            Label* miss,
                            Failure** failure);

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
  MaybeObject* GetCode(PropertyType type, String* name);
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

 private:
  MaybeObject* GetCode(PropertyType type, String* name);
};


class StoreStubCompiler: public StubCompiler {
 public:
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
  MUST_USE_RESULT MaybeObject* GetCode(PropertyType type, String* name);
};


class KeyedStoreStubCompiler: public StubCompiler {
 public:
  MaybeObject* CompileStoreField(JSObject* object,
                                 int index,
                                 Map* transition,
                                 String* name);

 private:
  MaybeObject* GetCode(PropertyType type, String* name);
};


// List of functions with custom constant call IC stubs.
//
// Installation of custom call generators for the selected builtins is
// handled by the bootstrapper.
//
// Each entry has a name of a global object property holding an object
// optionally followed by ".prototype" (this controls whether the
// generator is set on the object itself or, in case it's a function,
// on the its instance prototype), a name of a builtin function on the
// object (the one the generator is set for), and a name of the
// generator (used to build ids and generator function names).
#define CUSTOM_CALL_IC_GENERATORS(V)                \
  V(Array.prototype, push, ArrayPush)               \
  V(Array.prototype, pop, ArrayPop)                 \
  V(String.prototype, charCodeAt, StringCharCodeAt) \
  V(String.prototype, charAt, StringCharAt)         \
  V(String, fromCharCode, StringFromCharCode)       \
  V(Math, floor, MathFloor)                         \
  V(Math, abs, MathAbs)


class CallStubCompiler: public StubCompiler {
 public:
  enum {
#define DECLARE_CALL_GENERATOR_ID(ignored1, ignore2, name) \
    k##name##CallGenerator,
    CUSTOM_CALL_IC_GENERATORS(DECLARE_CALL_GENERATOR_ID)
#undef DECLARE_CALL_GENERATOR_ID
    kNumCallGenerators
  };

  CallStubCompiler(int argc,
                   InLoopFlag in_loop,
                   Code::Kind kind,
                   InlineCacheHolderFlag cache_holder);

  MUST_USE_RESULT MaybeObject* CompileCallField(JSObject* object,
                                                JSObject* holder,
                                                int index,
                                                String* name);
  MUST_USE_RESULT MaybeObject* CompileCallConstant(Object* object,
                                                   JSObject* holder,
                                                   JSFunction* function,
                                                   String* name,
                                                   CheckType check);
  MUST_USE_RESULT MaybeObject* CompileCallInterceptor(JSObject* object,
                                                      JSObject* holder,
                                                      String* name);
  MUST_USE_RESULT MaybeObject* CompileCallGlobal(JSObject* object,
                                                 GlobalObject* holder,
                                                 JSGlobalPropertyCell* cell,
                                                 JSFunction* function,
                                                 String* name);

  // Compiles a custom call constant/global IC using the generator
  // with given id. For constant calls cell is NULL.
  MUST_USE_RESULT MaybeObject* CompileCustomCall(int generator_id,
                                                 Object* object,
                                                 JSObject* holder,
                                                 JSGlobalPropertyCell* cell,
                                                 JSFunction* function,
                                                 String* name);

#define DECLARE_CALL_GENERATOR(ignored1, ignored2,  name)                      \
  MUST_USE_RESULT MaybeObject* Compile##name##Call(Object* object,             \
                                                   JSObject* holder,           \
                                                   JSGlobalPropertyCell* cell, \
                                                   JSFunction* function,       \
                                                   String* fname);
  CUSTOM_CALL_IC_GENERATORS(DECLARE_CALL_GENERATOR)
#undef DECLARE_CALL_GENERATOR

 private:
  const ParameterCount arguments_;
  const InLoopFlag in_loop_;
  const Code::Kind kind_;
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

  MUST_USE_RESULT MaybeObject* CompileConstructStub(SharedFunctionInfo* shared);

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
