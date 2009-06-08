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

#ifndef V8_ACCESSORS_H_
#define V8_ACCESSORS_H_

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
  V(ScriptEvalFromFunction)         \
  V(ScriptEvalFromPosition)         \
  V(ObjectPrototype)

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
  static Object* FunctionGetPrototype(Object* object, void*);
  static Object* FunctionSetPrototype(JSObject* object, Object* value, void*);
 private:
  // Accessor functions only used through the descriptor.
  static Object* FunctionGetLength(Object* object, void*);
  static Object* FunctionGetName(Object* object, void*);
  static Object* FunctionGetArguments(Object* object, void*);
  static Object* FunctionGetCaller(Object* object, void*);
  static Object* ArraySetLength(JSObject* object, Object* value, void*);
  static Object* ArrayGetLength(Object* object, void*);
  static Object* StringGetLength(Object* object, void*);
  static Object* ScriptGetName(Object* object, void*);
  static Object* ScriptGetId(Object* object, void*);
  static Object* ScriptGetSource(Object* object, void*);
  static Object* ScriptGetLineOffset(Object* object, void*);
  static Object* ScriptGetColumnOffset(Object* object, void*);
  static Object* ScriptGetData(Object* object, void*);
  static Object* ScriptGetType(Object* object, void*);
  static Object* ScriptGetCompilationType(Object* object, void*);
  static Object* ScriptGetLineEnds(Object* object, void*);
  static Object* ScriptGetContextData(Object* object, void*);
  static Object* ScriptGetEvalFromFunction(Object* object, void*);
  static Object* ScriptGetEvalFromPosition(Object* object, void*);
  static Object* ObjectGetPrototype(Object* receiver, void*);
  static Object* ObjectSetPrototype(JSObject* receiver, Object* value, void*);

  // Helper functions.
  static Object* FlattenNumber(Object* value);
  static Object* IllegalSetter(JSObject*, Object*, void*);
  static Object* IllegalGetAccessor(Object* object, void*);
  static Object* ReadOnlySetAccessor(JSObject*, Object* value, void*);
};

} }  // namespace v8::internal

#endif  // V8_ACCESSORS_H_
