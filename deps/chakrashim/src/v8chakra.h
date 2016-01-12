// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#pragma once
#include "v8.h"
#include "jsrtutils.h"

namespace v8 {

using jsrt::ContextShim;

struct ObjectTemplateData;

extern __declspec(thread) bool g_EnableDebug;
extern ArrayBuffer::Allocator* g_arrayBufferAllocator;

struct ObjectData {
 public:
  struct FieldValue {
   public:
    FieldValue() : value(nullptr), isRefValue(false) {}
    ~FieldValue() { Reset(); }

    void SetRef(JsValueRef ref);
    JsValueRef GetRef() const;
    bool IsRef() const;

    void SetPointer(void* ptr);
    void* GetPointer() const;

   private:
    void Reset();

    bool isRefValue;
    void* value;
  };

  JsValueRef objectInstance;
  Persistent<ObjectTemplate> objectTemplate;  // Original ObjectTemplate
  NamedPropertyGetterCallback namedPropertyGetter;
  NamedPropertySetterCallback namedPropertySetter;
  NamedPropertyQueryCallback namedPropertyQuery;
  NamedPropertyDeleterCallback namedPropertyDeleter;
  NamedPropertyEnumeratorCallback namedPropertyEnumerator;
  Persistent<Value> namedPropertyInterceptorData;
  IndexedPropertyGetterCallback indexedPropertyGetter;
  IndexedPropertySetterCallback indexedPropertySetter;
  IndexedPropertyQueryCallback indexedPropertyQuery;
  IndexedPropertyDeleterCallback indexedPropertyDeleter;
  IndexedPropertyEnumeratorCallback indexedPropertyEnumerator;
  Persistent<Value> indexedPropertyInterceptorData;
  int internalFieldCount;
  FieldValue* internalFields;

  ObjectData(ObjectTemplate* objectTemplate, ObjectTemplateData *templateData);
  void Dispose();
  static void CALLBACK FinalizeCallback(void *data);

  static FieldValue* GetInternalField(Object* object, int index);
};

struct TemplateData {
 protected:
  Persistent<Object> properties;
  virtual void CreateProperties();  // Allow properties to be created lazily

 public:
  virtual ~TemplateData() {}
  Object* EnsureProperties();
};

class Utils {
 public:
  static JsValueRef CALLBACK AccessorHandler(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);

  static JsValueRef CALLBACK GetCallback(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);
  static JsValueRef CALLBACK SetCallback(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);
  static JsValueRef CALLBACK DeletePropertyCallback(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);

  static JsValueRef HasPropertyHandler(
    JsValueRef *arguments,
    unsigned short argumentCount,
    bool checkInPrototype);
  static JsValueRef CALLBACK HasCallback(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);

  static JsValueRef GetPropertiesEnumeratorHandler(
    JsValueRef* arguments,
    unsigned int argumentsCount);
  static JsValueRef CALLBACK EnumerateCallback(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);

  static JsValueRef GetPropertiesHandler(
    JsValueRef* arguments,
    unsigned int argumentsCount,
    bool getFromPrototype);
  static JsValueRef CALLBACK OwnKeysCallback(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);
  static JsValueRef CALLBACK GetOwnPropertyDescriptorCallback(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);

  static void CALLBACK WeakReferenceCallbackWrapperCallback(
    JsRef ref, void *data);

  static JsValueRef CALLBACK ObjectPrototypeToStringShim(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState);

  static bool IsInstanceOf(Object* obj, ObjectTemplate* objectTemplate) {
    return obj->GetObjectTemplate() == objectTemplate;
  }

  static bool CheckSignature(Local<FunctionTemplate> receiver,
                             Local<Object> thisPointer,
                             Local<Object>* holder);

  template <class Func>
  static Local<Value> NewError(Handle<String> message, const Func& f);

  static JsErrorCode NewTypedArray(ContextShim::GlobalType constructorIndex,
                                   Handle<ArrayBuffer> array_buffer,
                                   size_t byte_offset, size_t length,
                                   JsValueRef* result);
  template <class T>
  static Local<T> NewTypedArray(ContextShim::GlobalType constructorIndex,
                                Handle<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
};


template <class T>
T FromMaybe(Maybe<T> maybe, const T& def = T()) {
  return maybe.FromMaybe(def);
}

template <class T>
Local<T> FromMaybe(MaybeLocal<T> maybe, const Local<T>& def = Local<T>()) {
  return maybe.FromMaybe(def);
}


}  // namespace v8
