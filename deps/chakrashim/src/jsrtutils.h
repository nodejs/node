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
#include "jsrtproxyutils.h"
#include "jsrtcontextshim.h"
#include "jsrtisolateshim.h"

#include "stdint.h"
#include "jsrtstringutils.h"
#include <assert.h>
#include <functional>

#define IfComFailError(v) \
  { \
  hr = (v) ; \
  if (FAILED(hr)) \
    { \
    goto error; \
    } \
  }

#define CHAKRA_UNIMPLEMENTED() jsrt::Unimplemented(__FUNCTION__)
#define CHAKRA_UNIMPLEMENTED_(message) jsrt::Unimplemented(message)

#define CHAKRA_VERIFY(expr) if (!(expr)) { \
  jsrt::Fatal("internal error %s(%d): %s", __FILE__, __LINE__, #expr); }

#ifdef DEBUG
#define CHAKRA_ASSERT(expr) assert(expr)
#else
#define CHAKRA_ASSERT(expr)
#endif

namespace jsrt {

JsErrorCode InitializePromise();

JsErrorCode UintToValue(uint32_t value, JsValueRef* result);

JsErrorCode GetProperty(JsValueRef ref,
                        JsValueRef propName,
                        JsValueRef *result);

JsErrorCode GetProperty(JsValueRef ref,
                        const wchar_t *propertyName,
                        JsValueRef *result);

JsErrorCode GetProperty(JsValueRef ref,
                        CachedPropertyIdRef cachedIdRef,
                        JsValueRef *result);

JsErrorCode GetProperty(JsValueRef ref,
                        JsPropertyIdRef propId,
                        int *intValue);

JsErrorCode SetProperty(JsValueRef ref,
                        JsValueRef propName,
                        JsValueRef propValue);

JsErrorCode SetProperty(JsValueRef ref,
                        CachedPropertyIdRef cachedIdRef,
                        JsValueRef propValue);

JsErrorCode DeleteIndexedProperty(JsValueRef object,
                                  unsigned int index);

JsErrorCode DeleteProperty(JsValueRef ref,
                           JsValueRef propName,
                           JsValueRef* result);

JsErrorCode GetOwnPropertyDescriptor(JsValueRef ref,
                                     JsValueRef prop,
                                     JsValueRef* result);

JsErrorCode IsValueInArray(JsValueRef arrayRef,
                           JsValueRef valueRef,
                           bool* result);

JsErrorCode IsValueInArray(
  JsValueRef arrayRef,
  JsValueRef valueRef,
  std::function<JsErrorCode(JsValueRef, JsValueRef, bool*)> comperator,
  bool* result);

JsErrorCode IsCaseInsensitiveStringValueInArray(JsValueRef arrayRef,
                                                JsValueRef valueRef,
                                                bool* result);

JsErrorCode IsZero(JsValueRef value,
                   bool *result);

JsErrorCode IsUndefined(JsValueRef value,
                        bool *result);

JsErrorCode HasOwnProperty(JsValueRef object,
                           JsValueRef prop,
                           JsValueRef *result);

JsErrorCode HasProperty(JsValueRef object,
                        JsValueRef prop,
                        bool *result);

JsErrorCode HasIndexedProperty(JsValueRef object,
                               unsigned int index,
                               bool *result);

JsErrorCode GetEnumerableNamedProperties(JsValueRef object,
                                         JsValueRef *result);

JsErrorCode GetEnumerableIndexedProperties(JsValueRef object,
                                           JsValueRef *result);

JsErrorCode GetIndexedOwnKeys(JsValueRef object,
                              JsValueRef *result);

JsErrorCode GetNamedOwnKeys(JsValueRef object,
                            JsValueRef *result);

JsErrorCode CreateEnumerationIterator(JsValueRef enumeration,
                                      JsValueRef *result);

JsErrorCode CreatePropertyDescriptorsEnumerationIterator(JsValueRef enumeration,
                                                         JsValueRef *result);

JsErrorCode ConcatArray(JsValueRef first,
                        JsValueRef second,
                        JsValueRef *result);

JsErrorCode CallProperty(JsValueRef ref,
                         CachedPropertyIdRef cachedIdRef,
                         JsValueRef *arguments,
                         unsigned short argumentCount,
                         JsValueRef *result);

JsErrorCode CallGetter(JsValueRef ref,
                       CachedPropertyIdRef cachedIdRef,
                       JsValueRef* result);

JsErrorCode CallGetter(JsValueRef ref,
                       CachedPropertyIdRef cachedIdRef,
                       int* result);

JsErrorCode GetPropertyOfGlobal(const wchar_t *propertyName,
                                JsValueRef *ref);

JsErrorCode SetPropertyOfGlobal(const wchar_t *propertyName,
                                JsValueRef ref);

JsValueRef GetNull();

JsValueRef GetUndefined();

JsValueRef GetTrue();

JsValueRef GetFalse();

JsErrorCode GetArrayLength(JsValueRef arrayRef,
                           unsigned int *arraySize);

bool InstanceOf(JsValueRef first,
                JsValueRef second);

JsErrorCode CloneObject(JsValueRef source,
                        JsValueRef target,
                        bool cloneProtoype = false);

JsErrorCode ConcatArray(JsValueRef first,
                        JsValueRef second,
                        JsValueRef *result);

JsErrorCode GetPropertyNames(JsValueRef object,
                             JsValueRef *namesArray);

JsErrorCode AddExternalData(JsValueRef ref,
                            JsPropertyIdRef externalDataPropertyId,
                            void *data,
                            JsFinalizeCallback onObjectFinalize);

JsErrorCode AddExternalData(JsValueRef ref,
                            void *data,
                            JsFinalizeCallback onObjectFinalize);

JsErrorCode GetExternalData(JsValueRef ref,
                            JsPropertyIdRef idRef,
                            void **data);

JsErrorCode GetExternalData(JsValueRef ref,
                            void **data);

JsErrorCode CreateFunctionWithExternalData(JsNativeFunction,
                                           void* data,
                                           JsFinalizeCallback onObjectFinalize,
                                           JsValueRef *function);

JsErrorCode ToString(JsValueRef ref,
                     JsValueRef * strRef,
                     const wchar_t** str,
                     bool alreadyString = false);

JsErrorCode IsValueMapIterator(JsValueRef value,
                               JsValueRef *resultRef);

JsErrorCode IsValueSetIterator(JsValueRef value,
                               JsValueRef *resultRef);

JsValueRef CALLBACK CollectGarbage(JsValueRef callee,
                                   bool isConstructCall,
                                   JsValueRef *arguments,
                                   unsigned short argumentCount,
                                   void *callbackState);

// the possible values for the property descriptor options
enum PropertyDescriptorOptionValues {
  True,
  False,
  None
};

PropertyDescriptorOptionValues GetPropertyDescriptorOptionValue(bool b);

JsErrorCode CreatePropertyDescriptor(
    PropertyDescriptorOptionValues writable,
    PropertyDescriptorOptionValues enumerable,
    PropertyDescriptorOptionValues configurable,
    JsValueRef value,
    JsValueRef getter,
    JsValueRef setter,
    JsValueRef *descriptor);

JsErrorCode CreatePropertyDescriptor(v8::PropertyAttribute attributes,
                                     JsValueRef value,
                                     JsValueRef getter,
                                     JsValueRef setter,
                                     JsValueRef *descriptor);

JsErrorCode DefineProperty(JsValueRef object,
                           const wchar_t * propertyName,
                           PropertyDescriptorOptionValues writable,
                           PropertyDescriptorOptionValues enumerable,
                           PropertyDescriptorOptionValues configurable,
                           JsValueRef value,
                           JsValueRef getter,
                           JsValueRef setter);

JsErrorCode DefineProperty(JsValueRef object,
                           JsPropertyIdRef propertyIdRef,
                           PropertyDescriptorOptionValues writable,
                           PropertyDescriptorOptionValues enumerable,
                           PropertyDescriptorOptionValues configurable,
                           JsValueRef value,
                           JsValueRef getter,
                           JsValueRef setter);

JsErrorCode GetPropertyIdFromName(JsValueRef nameRef,
                                  JsPropertyIdRef *idRef);

JsErrorCode GetPropertyIdFromValue(JsValueRef valueRef,
                                   JsPropertyIdRef *idRef);

JsErrorCode GetObjectConstructor(JsValueRef objectRef,
                                 JsValueRef *constructorRef);

JsErrorCode SetIndexedProperty(JsValueRef object,
                               unsigned int index,
                               JsValueRef value);

JsErrorCode GetIndexedProperty(JsValueRef object,
                               unsigned int index,
                               JsValueRef *value);

// CHAKRA-TODO : Currently Chakra's ParseScript doesn't support strictMode
// flag. As a workaround, prepend the script text with 'use strict'.
JsErrorCode ParseScript(const wchar_t *script,
                        JsSourceContext sourceContext,
                        const wchar_t *sourceUrl,
                        bool isStrictMode,
                        JsValueRef *result);

void Unimplemented(const char * message);

void Fatal(const char * format, ...);

// Arguments buffer for JsCallFunction
template <int STATIC_COUNT = 4>
class JsArguments {
 private:
  JsValueRef _local[STATIC_COUNT];
  JsValueRef* _args;

 public:
  explicit JsArguments(int count) {
    _args = count <= STATIC_COUNT ? _local : new JsValueRef[count];
  }

  ~JsArguments() {
    if (_args != _local) {
      delete [] _args;
    }
  }

  operator JsValueRef*() {
    return _args;
  }
};


// Helpers for JsCallFunction/JsConstructObject with undefined as arg0

template <class T>
JsErrorCode CallFunction(const T& api,
                         JsValueRef func,
                         JsValueRef* result) {
  JsValueRef args[] = { jsrt::GetUndefined() };
  return api(func, args, _countof(args), result);
}

template <class T>
JsErrorCode CallFunction(const T& api,
                         JsValueRef func, JsValueRef arg1,
                         JsValueRef* result) {
  JsValueRef args[] = { jsrt::GetUndefined(), arg1 };
  return api(func, args, _countof(args), result);
}

template <class T>
JsErrorCode CallFunction(const T& api,
                         JsValueRef func, JsValueRef arg1, JsValueRef arg2,
                         JsValueRef* result) {
  JsValueRef args[] = { jsrt::GetUndefined(), arg1, arg2 };
  return api(func, args, _countof(args), result);
}

inline JsErrorCode CallFunction(JsValueRef func,
                                JsValueRef* result) {
  return CallFunction(JsCallFunction, func, result);
}

inline JsErrorCode CallFunction(JsValueRef func, JsValueRef arg1,
                                JsValueRef* result) {
  return CallFunction(JsCallFunction, func, arg1, result);
}

inline JsErrorCode CallFunction(JsValueRef func,
                                JsValueRef arg1, JsValueRef arg2,
                                JsValueRef* result) {
  return CallFunction(JsCallFunction, func, arg1, arg2, result);
}

inline JsErrorCode ConstructObject(JsValueRef func,
                                   JsValueRef* result) {
  return CallFunction(JsConstructObject, func, result);
}

inline JsErrorCode ConstructObject(JsValueRef func, JsValueRef arg1,
                                   JsValueRef* result) {
  return CallFunction(JsConstructObject, func, arg1, result);
}

inline JsErrorCode ConstructObject(JsValueRef func,
                                   JsValueRef arg1, JsValueRef arg2,
                                   JsValueRef* result) {
  return CallFunction(JsConstructObject, func, arg1, arg2, result);
}


template <bool LIKELY,
          class JsConvertToValueFunc,
          class JsValueToNativeFunc,
          class T>
JsErrorCode ValueToNative(const JsConvertToValueFunc& JsConvertToValue,
                          const JsValueToNativeFunc& JsValueToNative,
                          JsValueRef value, T* nativeValue) {
  JsErrorCode error;

  // If LIKELY, try JsValueToNative first. Likely to succeed.
  if (LIKELY) {
    error = JsValueToNative(value, nativeValue);
    if (error != JsErrorInvalidArgument) {
      return error;
    }
  }

  // Perform JS conversion first, then to native.
  error = JsConvertToValue(value, &value);
  if (error != JsNoError) {
    return error;
  }
  return JsValueToNative(value, nativeValue);
}

inline JsErrorCode ValueToInt(JsValueRef value, int* intValue) {
  return ValueToNative</*LIKELY*/false>(
    JsConvertValueToNumber, JsNumberToInt, value, intValue);
}

inline JsErrorCode ValueToIntLikely(JsValueRef value, int* intValue) {
  return ValueToNative</*LIKELY*/true>(
    JsConvertValueToNumber, JsNumberToInt, value, intValue);
}

inline JsErrorCode ValueToDouble(JsValueRef value, double* dblValue) {
  return ValueToNative</*LIKELY*/false>(
    JsConvertValueToNumber, JsNumberToDouble, value, dblValue);
}

inline JsErrorCode ValueToDoubleLikely(JsValueRef value, double* dblValue) {
  return ValueToNative</*LIKELY*/true>(
    JsConvertValueToNumber, JsNumberToDouble, value, dblValue);
}
}  // namespace jsrt

