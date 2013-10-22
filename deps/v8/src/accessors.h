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

#ifndef V8_ACCESSORS_H_
#define V8_ACCESSORS_H_

#include "allocation.h"
#include "v8globals.h"

namespace v8 {
namespace internal {

// The list of accessor descriptors. This is a second-order macro
// taking a macro to be applied to all accessor descriptor names.
#define ACCESSOR_DESCRIPTOR_LIST(V) \
  V(FunctionPrototype)              \
  V(FunctionLength)                 \
  V(FunctionName)                   \
  V(FunctionArguments)              \
  V(FunctionCaller)                 \
  V(ArrayLength)                    \
  V(StringLength)                   \
  V(ScriptSource)                   \
  V(ScriptName)                     \
  V(ScriptId)                       \
  V(ScriptLineOffset)               \
  V(ScriptColumnOffset)             \
  V(ScriptData)                     \
  V(ScriptType)                     \
  V(ScriptCompilationType)          \
  V(ScriptLineEnds)                 \
  V(ScriptContextData)              \
  V(ScriptEvalFromScript)           \
  V(ScriptEvalFromScriptPosition)   \
  V(ScriptEvalFromFunctionName)

// Accessors contains all predefined proxy accessors.

class Accessors : public AllStatic {
 public:
  // Accessor descriptors.
#define ACCESSOR_DESCRIPTOR_DECLARATION(name) \
  static const AccessorDescriptor name;
  ACCESSOR_DESCRIPTOR_LIST(ACCESSOR_DESCRIPTOR_DECLARATION)
#undef ACCESSOR_DESCRIPTOR_DECLARATION

  enum DescriptorId {
#define ACCESSOR_DESCRIPTOR_DECLARATION(name) \
    k##name,
  ACCESSOR_DESCRIPTOR_LIST(ACCESSOR_DESCRIPTOR_DECLARATION)
#undef ACCESSOR_DESCRIPTOR_DECLARATION
    descriptorCount
  };

  // Accessor functions called directly from the runtime system.
  static Handle<Object> FunctionSetPrototype(Handle<JSFunction> object,
                                             Handle<Object> value);
  static Handle<Object> FunctionGetPrototype(Handle<JSFunction> object);
  static Handle<Object> FunctionGetArguments(Handle<JSFunction> object);

  // Accessor infos.
  static Handle<AccessorInfo> MakeModuleExport(
      Handle<String> name, int index, PropertyAttributes attributes);

 private:
  // Accessor functions only used through the descriptor.
  static MaybeObject* FunctionSetPrototype(Isolate* isolate,
                                           JSObject* object,
                                           Object*,
                                           void*);
  static MaybeObject* FunctionGetPrototype(Isolate* isolate,
                                           Object* object,
                                           void*);
  static MaybeObject* FunctionGetLength(Isolate* isolate,
                                        Object* object,
                                        void*);
  static MaybeObject* FunctionGetName(Isolate* isolate, Object* object, void*);
  static MaybeObject* FunctionGetArguments(Isolate* isolate,
                                           Object* object,
                                           void*);
  static MaybeObject* FunctionGetCaller(Isolate* isolate,
                                        Object* object,
                                        void*);
  static MaybeObject* ArraySetLength(Isolate* isolate,
                                     JSObject* object,
                                     Object*,
                                     void*);
  static MaybeObject* ArrayGetLength(Isolate* isolate, Object* object, void*);
  static MaybeObject* StringGetLength(Isolate* isolate, Object* object, void*);
  static MaybeObject* ScriptGetName(Isolate* isolate, Object* object, void*);
  static MaybeObject* ScriptGetId(Isolate* isolate, Object* object, void*);
  static MaybeObject* ScriptGetSource(Isolate* isolate, Object* object, void*);
  static MaybeObject* ScriptGetLineOffset(Isolate* isolate,
                                          Object* object,
                                          void*);
  static MaybeObject* ScriptGetColumnOffset(Isolate* isolate,
                                            Object* object,
                                            void*);
  static MaybeObject* ScriptGetData(Isolate* isolate, Object* object, void*);
  static MaybeObject* ScriptGetType(Isolate* isolate, Object* object, void*);
  static MaybeObject* ScriptGetCompilationType(Isolate* isolate,
                                               Object* object,
                                               void*);
  static MaybeObject* ScriptGetLineEnds(Isolate* isolate,
                                        Object* object,
                                        void*);
  static MaybeObject* ScriptGetContextData(Isolate* isolate,
                                           Object* object,
                                           void*);
  static MaybeObject* ScriptGetEvalFromScript(Isolate* isolate,
                                              Object* object,
                                              void*);
  static MaybeObject* ScriptGetEvalFromScriptPosition(Isolate* isolate,
                                                      Object* object,
                                                      void*);
  static MaybeObject* ScriptGetEvalFromFunctionName(Isolate* isolate,
                                                    Object* object,
                                                    void*);

  // Helper functions.
  static Object* FlattenNumber(Isolate* isolate, Object* value);
  static MaybeObject* IllegalSetter(Isolate* isolate,
                                    JSObject*,
                                    Object*,
                                    void*);
  static Object* IllegalGetAccessor(Isolate* isolate, Object* object, void*);
  static MaybeObject* ReadOnlySetAccessor(Isolate* isolate,
                                          JSObject*,
                                          Object* value,
                                          void*);
};

} }  // namespace v8::internal

#endif  // V8_ACCESSORS_H_
