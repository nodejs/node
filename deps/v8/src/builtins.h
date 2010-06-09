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

#ifndef V8_BUILTINS_H_
#define V8_BUILTINS_H_

namespace v8 {
namespace internal {

// Specifies extra arguments required by a C++ builtin.
enum BuiltinExtraArguments {
  NO_EXTRA_ARGUMENTS = 0,
  NEEDS_CALLED_FUNCTION = 1
};


// Define list of builtins implemented in C++.
#define BUILTIN_LIST_C(V)                                           \
  V(Illegal, NO_EXTRA_ARGUMENTS)                                    \
                                                                    \
  V(EmptyFunction, NO_EXTRA_ARGUMENTS)                              \
                                                                    \
  V(ArrayCodeGeneric, NO_EXTRA_ARGUMENTS)                           \
                                                                    \
  V(ArrayPush, NO_EXTRA_ARGUMENTS)                                  \
  V(ArrayPop, NO_EXTRA_ARGUMENTS)                                   \
  V(ArrayShift, NO_EXTRA_ARGUMENTS)                                 \
  V(ArrayUnshift, NO_EXTRA_ARGUMENTS)                               \
  V(ArraySlice, NO_EXTRA_ARGUMENTS)                                 \
  V(ArraySplice, NO_EXTRA_ARGUMENTS)                                \
  V(ArrayConcat, NO_EXTRA_ARGUMENTS)                                \
                                                                    \
  V(HandleApiCall, NEEDS_CALLED_FUNCTION)                           \
  V(FastHandleApiCall, NO_EXTRA_ARGUMENTS)                          \
  V(HandleApiCallConstruct, NEEDS_CALLED_FUNCTION)                  \
  V(HandleApiCallAsFunction, NO_EXTRA_ARGUMENTS)                    \
  V(HandleApiCallAsConstructor, NO_EXTRA_ARGUMENTS)


// Define list of builtins implemented in assembly.
#define BUILTIN_LIST_A(V)                                                 \
  V(ArgumentsAdaptorTrampoline, BUILTIN, UNINITIALIZED)                   \
  V(JSConstructCall,            BUILTIN, UNINITIALIZED)                   \
  V(JSConstructStubGeneric,     BUILTIN, UNINITIALIZED)                   \
  V(JSConstructStubApi,         BUILTIN, UNINITIALIZED)                   \
  V(JSEntryTrampoline,          BUILTIN, UNINITIALIZED)                   \
  V(JSConstructEntryTrampoline, BUILTIN, UNINITIALIZED)                   \
                                                                          \
  V(LoadIC_Miss,                BUILTIN, UNINITIALIZED)                   \
  V(KeyedLoadIC_Miss,           BUILTIN, UNINITIALIZED)                   \
  V(StoreIC_Miss,               BUILTIN, UNINITIALIZED)                   \
  V(KeyedStoreIC_Miss,          BUILTIN, UNINITIALIZED)                   \
                                                                          \
  V(LoadIC_Initialize,          LOAD_IC, UNINITIALIZED)                   \
  V(LoadIC_PreMonomorphic,      LOAD_IC, PREMONOMORPHIC)                  \
  V(LoadIC_Normal,              LOAD_IC, MONOMORPHIC)                     \
  V(LoadIC_ArrayLength,         LOAD_IC, MONOMORPHIC)                     \
  V(LoadIC_StringLength,        LOAD_IC, MONOMORPHIC)                     \
  V(LoadIC_FunctionPrototype,   LOAD_IC, MONOMORPHIC)                     \
  V(LoadIC_Megamorphic,         LOAD_IC, MEGAMORPHIC)                     \
                                                                          \
  V(KeyedLoadIC_Initialize,     KEYED_LOAD_IC, UNINITIALIZED)             \
  V(KeyedLoadIC_PreMonomorphic, KEYED_LOAD_IC, PREMONOMORPHIC)            \
  V(KeyedLoadIC_Generic,        KEYED_LOAD_IC, MEGAMORPHIC)               \
  V(KeyedLoadIC_String,         KEYED_LOAD_IC, MEGAMORPHIC)               \
  V(KeyedLoadIC_ExternalByteArray,          KEYED_LOAD_IC, MEGAMORPHIC)   \
  V(KeyedLoadIC_ExternalUnsignedByteArray,  KEYED_LOAD_IC, MEGAMORPHIC)   \
  V(KeyedLoadIC_ExternalShortArray,         KEYED_LOAD_IC, MEGAMORPHIC)   \
  V(KeyedLoadIC_ExternalUnsignedShortArray, KEYED_LOAD_IC, MEGAMORPHIC)   \
  V(KeyedLoadIC_ExternalIntArray,           KEYED_LOAD_IC, MEGAMORPHIC)   \
  V(KeyedLoadIC_ExternalUnsignedIntArray,   KEYED_LOAD_IC, MEGAMORPHIC)   \
  V(KeyedLoadIC_ExternalFloatArray,         KEYED_LOAD_IC, MEGAMORPHIC)   \
  V(KeyedLoadIC_IndexedInterceptor,         KEYED_LOAD_IC, MEGAMORPHIC)   \
                                                                          \
  V(StoreIC_Initialize,         STORE_IC, UNINITIALIZED)                  \
  V(StoreIC_ArrayLength,        STORE_IC, MONOMORPHIC)                    \
  V(StoreIC_Megamorphic,        STORE_IC, MEGAMORPHIC)                    \
                                                                          \
  V(KeyedStoreIC_Initialize,    KEYED_STORE_IC, UNINITIALIZED)            \
  V(KeyedStoreIC_Generic,       KEYED_STORE_IC, MEGAMORPHIC)              \
  V(KeyedStoreIC_ExternalByteArray,          KEYED_STORE_IC, MEGAMORPHIC) \
  V(KeyedStoreIC_ExternalUnsignedByteArray,  KEYED_STORE_IC, MEGAMORPHIC) \
  V(KeyedStoreIC_ExternalShortArray,         KEYED_STORE_IC, MEGAMORPHIC) \
  V(KeyedStoreIC_ExternalUnsignedShortArray, KEYED_STORE_IC, MEGAMORPHIC) \
  V(KeyedStoreIC_ExternalIntArray,           KEYED_STORE_IC, MEGAMORPHIC) \
  V(KeyedStoreIC_ExternalUnsignedIntArray,   KEYED_STORE_IC, MEGAMORPHIC) \
  V(KeyedStoreIC_ExternalFloatArray,         KEYED_STORE_IC, MEGAMORPHIC) \
                                                                          \
  /* Uses KeyedLoadIC_Initialize; must be after in list. */               \
  V(FunctionCall,               BUILTIN, UNINITIALIZED)                   \
  V(FunctionApply,              BUILTIN, UNINITIALIZED)                   \
                                                                          \
  V(ArrayCode,                  BUILTIN, UNINITIALIZED)                   \
  V(ArrayConstructCode,         BUILTIN, UNINITIALIZED)

#ifdef ENABLE_DEBUGGER_SUPPORT
// Define list of builtins used by the debugger implemented in assembly.
#define BUILTIN_LIST_DEBUG_A(V)                                \
  V(Return_DebugBreak,          BUILTIN, DEBUG_BREAK)          \
  V(ConstructCall_DebugBreak,   BUILTIN, DEBUG_BREAK)          \
  V(StubNoRegisters_DebugBreak, BUILTIN, DEBUG_BREAK)          \
  V(LoadIC_DebugBreak,          LOAD_IC, DEBUG_BREAK)          \
  V(KeyedLoadIC_DebugBreak,     KEYED_LOAD_IC, DEBUG_BREAK)    \
  V(StoreIC_DebugBreak,         STORE_IC, DEBUG_BREAK)         \
  V(KeyedStoreIC_DebugBreak,    KEYED_STORE_IC, DEBUG_BREAK)   \
  V(Slot_DebugBreak,            BUILTIN, DEBUG_BREAK)          \
  V(PlainReturn_LiveEdit,       BUILTIN, DEBUG_BREAK)          \
  V(FrameDropper_LiveEdit,      BUILTIN, DEBUG_BREAK)
#else
#define BUILTIN_LIST_DEBUG_A(V)
#endif

// Define list of builtins implemented in JavaScript.
#define BUILTINS_LIST_JS(V)              \
  V(EQUALS, 1)                           \
  V(STRICT_EQUALS, 1)                    \
  V(COMPARE, 2)                          \
  V(ADD, 1)                              \
  V(SUB, 1)                              \
  V(MUL, 1)                              \
  V(DIV, 1)                              \
  V(MOD, 1)                              \
  V(BIT_OR, 1)                           \
  V(BIT_AND, 1)                          \
  V(BIT_XOR, 1)                          \
  V(UNARY_MINUS, 0)                      \
  V(BIT_NOT, 0)                          \
  V(SHL, 1)                              \
  V(SAR, 1)                              \
  V(SHR, 1)                              \
  V(DELETE, 1)                           \
  V(IN, 1)                               \
  V(INSTANCE_OF, 1)                      \
  V(GET_KEYS, 0)                         \
  V(FILTER_KEY, 1)                       \
  V(CALL_NON_FUNCTION, 0)                \
  V(CALL_NON_FUNCTION_AS_CONSTRUCTOR, 0) \
  V(TO_OBJECT, 0)                        \
  V(TO_NUMBER, 0)                        \
  V(TO_STRING, 0)                        \
  V(STRING_ADD_LEFT, 1)                  \
  V(STRING_ADD_RIGHT, 1)                 \
  V(APPLY_PREPARE, 1)                    \
  V(APPLY_OVERFLOW, 1)


class ObjectVisitor;


class Builtins : public AllStatic {
 public:
  // Generate all builtin code objects. Should be called once during
  // VM initialization.
  static void Setup(bool create_heap_objects);
  static void TearDown();

  // Garbage collection support.
  static void IterateBuiltins(ObjectVisitor* v);

  // Disassembler support.
  static const char* Lookup(byte* pc);

  enum Name {
#define DEF_ENUM_C(name, ignore) name,
#define DEF_ENUM_A(name, kind, state) name,
    BUILTIN_LIST_C(DEF_ENUM_C)
    BUILTIN_LIST_A(DEF_ENUM_A)
    BUILTIN_LIST_DEBUG_A(DEF_ENUM_A)
#undef DEF_ENUM_C
#undef DEF_ENUM_A
    builtin_count
  };

  enum CFunctionId {
#define DEF_ENUM_C(name, ignore) c_##name,
    BUILTIN_LIST_C(DEF_ENUM_C)
#undef DEF_ENUM_C
    cfunction_count
  };

  enum JavaScript {
#define DEF_ENUM(name, ignore) name,
    BUILTINS_LIST_JS(DEF_ENUM)
#undef DEF_ENUM
    id_count
  };

  static Code* builtin(Name name) {
    // Code::cast cannot be used here since we access builtins
    // during the marking phase of mark sweep. See IC::Clear.
    return reinterpret_cast<Code*>(builtins_[name]);
  }

  static Address builtin_address(Name name) {
    return reinterpret_cast<Address>(&builtins_[name]);
  }

  static Address c_function_address(CFunctionId id) {
    return c_functions_[id];
  }

  static const char* GetName(JavaScript id) { return javascript_names_[id]; }
  static int GetArgumentsCount(JavaScript id) { return javascript_argc_[id]; }
  static Handle<Code> GetCode(JavaScript id, bool* resolved);
  static int NumberOfJavaScriptBuiltins() { return id_count; }

 private:
  // The external C++ functions called from the code.
  static Address c_functions_[cfunction_count];

  // Note: These are always Code objects, but to conform with
  // IterateBuiltins() above which assumes Object**'s for the callback
  // function f, we use an Object* array here.
  static Object* builtins_[builtin_count];
  static const char* names_[builtin_count];
  static const char* javascript_names_[id_count];
  static int javascript_argc_[id_count];

  static void Generate_Adaptor(MacroAssembler* masm,
                               CFunctionId id,
                               BuiltinExtraArguments extra_args);
  static void Generate_JSConstructCall(MacroAssembler* masm);
  static void Generate_JSConstructStubGeneric(MacroAssembler* masm);
  static void Generate_JSConstructStubApi(MacroAssembler* masm);
  static void Generate_JSEntryTrampoline(MacroAssembler* masm);
  static void Generate_JSConstructEntryTrampoline(MacroAssembler* masm);
  static void Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm);

  static void Generate_FunctionCall(MacroAssembler* masm);
  static void Generate_FunctionApply(MacroAssembler* masm);

  static void Generate_ArrayCode(MacroAssembler* masm);
  static void Generate_ArrayConstructCode(MacroAssembler* masm);
};

} }  // namespace v8::internal

#endif  // V8_BUILTINS_H_
