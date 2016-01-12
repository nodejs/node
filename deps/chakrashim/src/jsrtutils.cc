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

#include "jsrtutils.h"
#include <functional>
#include <algorithm>

namespace jsrt {

JsErrorCode UintToValue(uint32_t value, JsValueRef* result) {
  if (static_cast<int>(value) >= 0) {
    return JsIntToNumber(static_cast<int>(value), result);
  }

  // Otherwise doesn't fit int, use double
  return JsDoubleToNumber(value, result);
}

JsErrorCode GetProperty(JsValueRef ref,
                        JsValueRef propName,
                        JsValueRef *result) {
  JsPropertyIdRef idRef;
  JsErrorCode error;

  error = GetPropertyIdFromName(propName, &idRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsGetProperty(ref, idRef, result);

  return error;
}

JsErrorCode GetProperty(JsValueRef ref,
                        const wchar_t *propertyName,
                        JsValueRef *result) {
  JsPropertyIdRef idRef;
  JsErrorCode error;

  error = JsGetPropertyIdFromName(propertyName, &idRef);

  if (error != JsNoError) {
    return error;
  }

  error = JsGetProperty(ref, idRef, result);

  return error;
}

JsErrorCode GetProperty(JsValueRef ref,
                        CachedPropertyIdRef cachedIdRef,
                        JsValueRef *result) {
  JsPropertyIdRef idRef =
    IsolateShim::GetCurrent()->GetCachedPropertyIdRef(cachedIdRef);

  return JsGetProperty(ref, idRef, result);
}

JsErrorCode GetProperty(JsValueRef ref,
                        JsPropertyIdRef propId,
                        int *intValue) {
  JsValueRef value;
  JsErrorCode error = JsGetProperty(ref, propId, &value);
  if (error != JsNoError) {
    return error;
  }

  return ValueToIntLikely(value, intValue);
}

JsErrorCode SetProperty(JsValueRef ref,
                        CachedPropertyIdRef cachedIdRef,
                        JsValueRef propValue) {
  JsPropertyIdRef idRef =
    IsolateShim::GetCurrent()->GetCachedPropertyIdRef(cachedIdRef);
  return JsSetProperty(ref, idRef, propValue, false);
}

JsErrorCode SetProperty(JsValueRef ref,
                        JsValueRef propName,
                        JsValueRef propValue) {
  JsPropertyIdRef idRef;
  JsErrorCode error;

  error = GetPropertyIdFromName(propName, &idRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsSetProperty(ref, idRef, propValue, false);

  return error;
}

JsErrorCode DeleteProperty(JsValueRef ref,
                           JsValueRef propName,
                           JsValueRef* result) {
  JsPropertyIdRef idRef;
  JsErrorCode error;

  error = GetPropertyIdFromName(propName, &idRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsDeleteProperty(ref, idRef, false, result);

  return error;
}

JsErrorCode CallProperty(JsValueRef ref,
                         CachedPropertyIdRef cachedIdRef,
                         JsValueRef *arguments,
                         unsigned short argumentCount,
                         JsValueRef *result) {
  JsValueRef propertyRef;
  JsErrorCode error;

  error = JsGetProperty(ref,
    jsrt::IsolateShim::GetCurrent()->GetCachedPropertyIdRef(cachedIdRef),
    &propertyRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsCallFunction(propertyRef, arguments, argumentCount, result);
  return error;
}

JsErrorCode CallGetter(JsValueRef ref,
                       CachedPropertyIdRef cachedIdRef,
                       JsValueRef* result) {
  JsValueRef args[] = { ref };
  return CallProperty(ref, cachedIdRef, args, _countof(args), result);
}

JsErrorCode CallGetter(JsValueRef ref,
                       CachedPropertyIdRef cachedIdRef,
                       int* result) {
  JsValueRef value;
  JsErrorCode error = CallGetter(ref, cachedIdRef, &value);
  if (error != JsNoError) {
    return error;
  }

  return ValueToIntLikely(value, result);
}

JsErrorCode GetPropertyOfGlobal(const wchar_t *propertyName,
                                JsValueRef *ref) {
  JsErrorCode error;
  JsPropertyIdRef propertyIdRef, globalRef;

  error = JsGetPropertyIdFromName(propertyName, &propertyIdRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsGetGlobalObject(&globalRef);

  if (error != JsNoError) {
    return error;
  }

  return JsGetProperty(globalRef, propertyIdRef, ref);
}

JsErrorCode SetPropertyOfGlobal(const wchar_t *propertyName,
                                JsValueRef ref) {
  JsErrorCode error;
  JsPropertyIdRef propertyIdRef, globalRef;

  error = JsGetPropertyIdFromName(propertyName, &propertyIdRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsGetGlobalObject(&globalRef);

  if (error != JsNoError) {
    return error;
  }

  return JsSetProperty(globalRef, propertyIdRef, ref, false);
}

JsValueRef GetTrue() {
  return ContextShim::GetCurrent()->GetTrue();
}

JsValueRef GetFalse() {
  return ContextShim::GetCurrent()->GetFalse();
}

JsValueRef GetUndefined() {
  return ContextShim::GetCurrent()->GetUndefined();
}

JsValueRef GetNull() {
  return ContextShim::GetCurrent()->GetNull();
}

JsErrorCode GetArrayLength(JsValueRef arrayRef,
                           unsigned int *arraySize) {
  JsErrorCode error;

  JsPropertyIdRef arrayLengthPropertyIdRef =
    IsolateShim::GetCurrent()->GetCachedPropertyIdRef(
      CachedPropertyIdRef::length);
  JsValueRef lengthRef;

  error = JsGetProperty(arrayRef, arrayLengthPropertyIdRef, &lengthRef);
  if (error != JsNoError) {
    return error;
  }

  double sizeInDouble;
  error = JsNumberToDouble(lengthRef, &sizeInDouble);
  *arraySize = static_cast<unsigned int>(sizeInDouble);

  return error;
}

bool InstanceOf(JsValueRef first,
                JsValueRef second) {
  bool result;
  return JsInstanceOf(first, second, &result) == JsNoError && result;
}

JsErrorCode CloneObject(JsValueRef source,
                        JsValueRef target,
                        bool clonePrototype) {
  JsValueRef cloneObjectFunction =
    ContextShim::GetCurrent()->GetCloneObjectFunction();

  JsValueRef resultRef;
  JsErrorCode error = CallFunction(cloneObjectFunction,
                                   source, target, &resultRef);
  if (error != JsNoError) {
    return error;
  }

  if (clonePrototype) {
    JsValueRef prototypeRef;
    JsErrorCode error = JsGetPrototype(source, &prototypeRef);
    if (error != JsNoError) {
      return error;
    }
    error = JsSetPrototype(target, prototypeRef);
  }

  return error;
}

JsErrorCode HasOwnProperty(JsValueRef object,
                           JsValueRef prop,
                           JsValueRef *result) {
  JsValueRef hasOwnPropertyFunction =
    ContextShim::GetCurrent()->GetGlobalPrototypeFunction(
      ContextShim::GlobalPrototypeFunction::Object_hasOwnProperty);

  JsValueRef args[] = { object, prop };
  return JsCallFunction(hasOwnPropertyFunction, args, _countof(args), result);
}

JsErrorCode IsValueInArray(
    JsValueRef arrayRef,
    JsValueRef valueRef,
    std::function<JsErrorCode(JsValueRef, JsValueRef, bool*)> comparator,
    bool* result) {
  JsErrorCode error;
  unsigned int length;
  *result = false;
  error = GetArrayLength(arrayRef, &length);
  if (error != JsNoError) {
    return error;
  }

  for (unsigned int index = 0; index < length; index++) {
    JsValueRef indexValue;
    error = JsIntToNumber(index, &indexValue);
    if (error != JsNoError) {
      return error;
    }

    JsValueRef itemRef;
    error = JsGetIndexedProperty(arrayRef, indexValue, &itemRef);
    if (error != JsNoError) {
      return error;
    }

    if (comparator != nullptr) {
      error = comparator(valueRef, itemRef, result);
    } else {
      error = JsEquals(itemRef, valueRef, result);
    }

    if (error != JsNoError) {
      return error;
    }

    if (*result)
      return JsNoError;
  }

  return JsNoError;
}

JsErrorCode IsValueInArray(JsValueRef arrayRef,
                           JsValueRef valueRef,
                           bool* result) {
  return IsValueInArray(arrayRef, valueRef, nullptr, result);
}

JsErrorCode IsCaseInsensitiveStringValueInArray(JsValueRef arrayRef,
                                                JsValueRef valueRef,
                                                bool* result) {
  return IsValueInArray(arrayRef, valueRef, [=](
      JsValueRef first, JsValueRef second, bool* areEqual) -> JsErrorCode {
    JsValueType type;
    *areEqual = false;

    JsErrorCode error = JsGetValueType(first, &type);
    if (error != JsNoError) {
      return error;
    }
    if (type != JsString) {
      return JsNoError;
    }

    error = JsGetValueType(second, &type);
    if (error != JsNoError) {
      return error;
    }
    if (type != JsString) {
      return JsNoError;
    }

    const wchar_t* firstPtr;
    size_t firstLength;

    error = JsStringToPointer(first, &firstPtr, &firstLength);
    if (error != JsNoError) {
      return error;
    }

    const wchar_t* secondPtr;
    size_t secondLength;

    error = JsStringToPointer(second, &secondPtr, &secondLength);
    if (error != JsNoError) {
      return error;
    }

    size_t maxCount = min(firstLength, secondLength);
    *areEqual = (_wcsnicmp(firstPtr, secondPtr, maxCount) == 0);
    return JsNoError;
  },
                        result);
}

JsErrorCode GetOwnPropertyDescriptor(JsValueRef ref,
                                     JsValueRef prop,
                                     JsValueRef* result) {
  return CallFunction(
    ContextShim::GetCurrent()->GetGetOwnPropertyDescriptorFunction(),
    ref, prop, result);
}

JsErrorCode IsZero(JsValueRef value,
                   bool *result) {
  return JsEquals(value, ContextShim::GetCurrent()->GetZero(), result);
}

JsErrorCode IsUndefined(JsValueRef value,
                        bool *result) {
  return JsEquals(value, GetUndefined(), result);
}

JsErrorCode GetEnumerableNamedProperties(JsValueRef object,
                                         JsValueRef *result) {
  return CallFunction(
    ContextShim::GetCurrent()->GetGetEnumerableNamedPropertiesFunction(),
    object, result);
}

JsErrorCode GetEnumerableIndexedProperties(JsValueRef object,
                                           JsValueRef *result) {
  return CallFunction(
    ContextShim::GetCurrent()->GetGetEnumerableIndexedPropertiesFunction(),
    object, result);
}

JsErrorCode GetIndexedOwnKeys(JsValueRef object,
                              JsValueRef *result) {
  return CallFunction(
    ContextShim::GetCurrent()->GetGetIndexedOwnKeysFunction(),
    object, result);
}

JsErrorCode GetNamedOwnKeys(JsValueRef object,
                            JsValueRef *result) {
  return CallFunction(
    ContextShim::GetCurrent()->GetGetNamedOwnKeysFunction(),
    object, result);
}

JsErrorCode ConcatArray(JsValueRef first,
                        JsValueRef second,
                        JsValueRef *result) {
  JsValueRef args[] = { first, second };

  return CallProperty(first,
                      CachedPropertyIdRef::concat,
                      args, _countof(args), result);
}

JsErrorCode CreateEnumerationIterator(JsValueRef enumeration,
                                      JsValueRef *result) {
  return CallFunction(
    ContextShim::GetCurrent()->GetCreateEnumerationIteratorFunction(),
    enumeration, result);
}

JsErrorCode CreatePropertyDescriptorsEnumerationIterator(JsValueRef enumeration,
                                                         JsValueRef *result) {
  return CallFunction(
    ContextShim::GetCurrent()
      ->GetCreatePropertyDescriptorsEnumerationIteratorFunction(),
    enumeration, result);
}

JsErrorCode GetPropertyNames(JsValueRef object,
                             JsValueRef *result) {
  return CallFunction(
    ContextShim::GetCurrent()->GetGetPropertyNamesFunction(),
    object, result);
}

JsErrorCode AddExternalData(JsValueRef ref,
                            JsPropertyIdRef externalDataPropertyId,
                            void *data,
                            JsFinalizeCallback onObjectFinalize) {
  JsErrorCode error;

  JsValueRef externalObjectRef;
  error = JsCreateExternalObject(data, onObjectFinalize, &externalObjectRef);
  if (error != JsNoError) {
    return error;
  }

  error = DefineProperty(ref,
                         externalDataPropertyId,
                         PropertyDescriptorOptionValues::False,
                         PropertyDescriptorOptionValues::False,
                         PropertyDescriptorOptionValues::False,
                         externalObjectRef,
                         JS_INVALID_REFERENCE, JS_INVALID_REFERENCE);
  return error;
}

JsErrorCode AddExternalData(JsValueRef ref,
                            void *data,
                            JsFinalizeCallback onObjectFinalize) {
  IsolateShim* iso = IsolateShim::GetCurrent();
  JsPropertyIdRef propId = iso->GetCachedSymbolPropertyIdRef(
    CachedSymbolPropertyIdRef::__external__);

  return AddExternalData(ref, propId, data, onObjectFinalize);
}

JsErrorCode GetExternalData(JsValueRef ref,
                            JsPropertyIdRef idRef,
                            void **data) {
  JsErrorCode error;

  JsValueRef externalObject;
  error = JsGetProperty(ref, idRef, &externalObject);
  if (error != JsNoError) {
    return error;
  }

  error = JsGetExternalData(externalObject, data);
  if (error == JsErrorInvalidArgument) {
    *data = nullptr;
    error = JsNoError;
  }

  return error;
}

JsErrorCode GetExternalData(JsValueRef ref,
                            void **data) {
  IsolateShim* iso = IsolateShim::GetCurrent();
  JsPropertyIdRef propId = iso->GetCachedSymbolPropertyIdRef(
    CachedSymbolPropertyIdRef::__external__);

  return GetExternalData(ref, propId, data);
}

JsErrorCode CreateFunctionWithExternalData(
    JsNativeFunction nativeFunction,
    void* data,
    JsFinalizeCallback onObjectFinalize,
    JsValueRef *function) {
  JsErrorCode error;
  error = JsCreateFunction(nativeFunction, nullptr, function);
  if (error != JsNoError) {
    return error;
  }

  error = AddExternalData(*function, data, onObjectFinalize);
  return error;
}

JsErrorCode ToString(JsValueRef ref,
                     JsValueRef * strRef,
                     const wchar_t** str,
                     bool alreadyString) {
  // just a dummy here
  size_t size;
  JsErrorCode error;

  // call convert only if needed
  if (alreadyString) {
    *strRef = ref;
  } else {
    error = JsConvertValueToString(ref, strRef);
    if (error != JsNoError) {
      return error;
    }
  }

  error = JsStringToPointer(*strRef, str, &size);
  return error;
}

JsErrorCode IsValueMapIterator(JsValueRef value,
                        JsValueRef *resultRef) {
  return CallFunction(
    ContextShim::GetCurrent()->GetIsMapIteratorFunction(),
    value, resultRef);
}


JsErrorCode IsValueSetIterator(JsValueRef value,
                               JsValueRef *resultRef) {
  return CallFunction(
    ContextShim::GetCurrent()->GetIsSetIteratorFunction(),
    value, resultRef);
}

PropertyDescriptorOptionValues GetPropertyDescriptorOptionValue(bool b) {
  return b ?
    PropertyDescriptorOptionValues::True :
    PropertyDescriptorOptionValues::False;
}

JsErrorCode CreatePropertyDescriptor(
    PropertyDescriptorOptionValues writable,
    PropertyDescriptorOptionValues enumerable,
    PropertyDescriptorOptionValues configurable,
    JsValueRef value,
    JsValueRef getter,
    JsValueRef setter,
    JsValueRef *descriptor) {
  JsErrorCode error;
  error = JsCreateObject(descriptor);
  if (error != JsNoError) {
    return error;
  }

  IsolateShim * isolateShim = IsolateShim::GetCurrent();
  ContextShim * contextShim = isolateShim->GetCurrentContextShim();
  JsValueRef trueRef = contextShim->GetTrue();
  JsValueRef falseRef = contextShim->GetFalse();

  // set writable
  if (writable != PropertyDescriptorOptionValues::None) {
    JsPropertyIdRef writablePropertyIdRef =
      isolateShim->GetCachedPropertyIdRef(CachedPropertyIdRef::writable);
    JsValueRef writableRef =
      (writable == PropertyDescriptorOptionValues::True) ? trueRef : falseRef;
    error = JsSetProperty(*descriptor,
                          writablePropertyIdRef, writableRef, false);
    if (error != JsNoError) {
      return error;
    }
  }

  // set enumerable
  if (enumerable != PropertyDescriptorOptionValues::None) {
    JsPropertyIdRef enumerablePropertyIdRef =
      isolateShim->GetCachedPropertyIdRef(CachedPropertyIdRef::enumerable);
    JsValueRef enumerableRef =
      (enumerable == PropertyDescriptorOptionValues::True) ? trueRef : falseRef;
    error = JsSetProperty(*descriptor,
                          enumerablePropertyIdRef, enumerableRef, false);
    if (error != JsNoError) {
      return error;
    }
  }

  // set configurable
  if (configurable != PropertyDescriptorOptionValues::None) {
    JsPropertyIdRef configurablePropertyIdRef =
      isolateShim->GetCachedPropertyIdRef(CachedPropertyIdRef::configurable);
    JsValueRef configurableRef =
      (configurable == PropertyDescriptorOptionValues::True) ?
        trueRef : falseRef;
    error = JsSetProperty(*descriptor,
                          configurablePropertyIdRef, configurableRef, false);
    if (error != JsNoError) {
      return error;
    }
  }

  // set value
  if (value != JS_INVALID_REFERENCE) {
    JsPropertyIdRef valuePropertyIdRef =
      isolateShim->GetCachedPropertyIdRef(CachedPropertyIdRef::value);
    error = JsSetProperty(*descriptor, valuePropertyIdRef, value, false);
    if (error != JsNoError) {
      return error;
    }
  }

  // set getter if needed
  if (getter != JS_INVALID_REFERENCE) {
    JsPropertyIdRef getterPropertyIdRef =
      isolateShim->GetCachedPropertyIdRef(CachedPropertyIdRef::get);
    error = JsSetProperty(*descriptor, getterPropertyIdRef, getter, false);
    if (error != JsNoError) {
      return error;
    }
  }

  // set setter if needed
  if (setter != JS_INVALID_REFERENCE) {
    JsPropertyIdRef setterPropertyIdRef =
      isolateShim->GetCachedPropertyIdRef(CachedPropertyIdRef::set);
    error = JsSetProperty(*descriptor, setterPropertyIdRef, setter, false);
    if (error != JsNoError) {
      return error;
    }
  }

  return JsNoError;
}

JsErrorCode CreatePropertyDescriptor(v8::PropertyAttribute attributes,
                                     JsValueRef value,
                                     JsValueRef getter,
                                     JsValueRef setter,
                                     JsValueRef *descriptor) {
  return CreatePropertyDescriptor(
    GetPropertyDescriptorOptionValue(!(attributes & v8::ReadOnly)),
    GetPropertyDescriptorOptionValue(!(attributes & v8::DontEnum)),
    GetPropertyDescriptorOptionValue(!(attributes & v8::DontDelete)),
    value,
    JS_INVALID_REFERENCE,
    JS_INVALID_REFERENCE,
    descriptor);
}

JsErrorCode DefineProperty(JsValueRef object,
                           JsPropertyIdRef propertyIdRef,
                           PropertyDescriptorOptionValues writable,
                           PropertyDescriptorOptionValues enumerable,
                           PropertyDescriptorOptionValues configurable,
                           JsValueRef value,
                           JsValueRef getter,
                           JsValueRef setter) {
  JsValueRef descriptor;
  JsErrorCode error;
  error = CreatePropertyDescriptor(
    writable, enumerable, configurable, value, getter, setter, &descriptor);
  if (error != JsNoError) {
    return error;
  }

  bool result;
  error = JsDefineProperty(object, propertyIdRef, descriptor, &result);

  if (error == JsNoError && !result) {
    return JsErrorInvalidArgument;
  }
  return error;
}

JsErrorCode DefineProperty(JsValueRef object,
                           const wchar_t * propertyName,
                           PropertyDescriptorOptionValues writable,
                           PropertyDescriptorOptionValues enumerable,
                           PropertyDescriptorOptionValues configurable,
                           JsValueRef value,
                           JsValueRef getter,
                           JsValueRef setter) {
  JsErrorCode error;
  JsPropertyIdRef propertyIdRef;

  error = JsGetPropertyIdFromName(propertyName, &propertyIdRef);
  if (error != JsNoError) {
    return error;
  }

  error = DefineProperty(
    object, propertyIdRef, writable, enumerable, configurable, value,
    getter, setter);
  return error;
}

JsErrorCode GetPropertyIdFromName(JsValueRef nameRef,
                                  JsPropertyIdRef *idRef) {
  JsErrorCode error;
  const wchar_t *propertyName;
  size_t propertyNameSize;

  // Expect the name be either a String or a Symbol.
  error = JsStringToPointer(nameRef, &propertyName, &propertyNameSize);
  if (error != JsNoError) {
    if (error == JsErrorInvalidArgument) {
      error = JsGetPropertyIdFromSymbol(nameRef, idRef);
      if (error == JsErrorPropertyNotSymbol) {
        error = JsErrorInvalidArgument; // Neither String nor Symbol
      }
    }
  } else {
    error = JsGetPropertyIdFromName(propertyName, idRef);
  }

  return error;
}

JsErrorCode GetPropertyIdFromValue(JsValueRef valueRef,
                                   JsPropertyIdRef *idRef) {
  JsErrorCode error;

  error = GetPropertyIdFromName(valueRef, idRef);
  if (error == JsErrorInvalidArgument) {
    error = JsConvertValueToString(valueRef, &valueRef);
    if (error != JsNoError) {
      return error;
    }

    error = GetPropertyIdFromName(valueRef, idRef);
  }

  return error;
}

JsErrorCode GetObjectConstructor(JsValueRef objectRef,
                                 JsValueRef *constructorRef) {
  IsolateShim* iso = IsolateShim::GetCurrent();
  JsPropertyIdRef constructorPropertyIdRef = iso->GetCachedPropertyIdRef(
    CachedPropertyIdRef::constructor);

  return JsGetProperty(objectRef, constructorPropertyIdRef, constructorRef);
}

JsErrorCode SetIndexedProperty(JsValueRef object,
                               unsigned int index,
                               JsValueRef value) {
  JsErrorCode error;
  JsValueRef indexRef;
  error = UintToValue(index, &indexRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsSetIndexedProperty(object, indexRef, value);
  return error;
}

JsErrorCode GetIndexedProperty(JsValueRef object,
                               unsigned int index,
                               JsValueRef *value) {
  JsErrorCode error;
  JsValueRef indexRef;
  error = UintToValue(index, &indexRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsGetIndexedProperty(object, indexRef, value);
  return error;
}

JsErrorCode DeleteIndexedProperty(JsValueRef object,
                                  unsigned int index) {
  JsErrorCode error;
  JsValueRef indexRef;
  error = UintToValue(index, &indexRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsDeleteIndexedProperty(object, indexRef);
  return error;
}

JsErrorCode HasProperty(JsValueRef object,
                        JsValueRef propName,
                        bool *result) {
  JsPropertyIdRef idRef;
  JsErrorCode error;

  error = GetPropertyIdFromName(propName, &idRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsHasProperty(object, idRef, result);

  return error;
}

JsErrorCode HasIndexedProperty(JsValueRef object,
                               unsigned int index,
                               bool *result) {
  JsErrorCode error;
  JsValueRef indexRef;
  error = UintToValue(index, &indexRef);
  if (error != JsNoError) {
    return error;
  }

  error = JsHasIndexedProperty(object, indexRef, result);
  return error;
}

JsErrorCode ParseScript(const wchar_t *script,
                        JsSourceContext sourceContext,
                        const wchar_t *sourceUrl,
                        bool isStrictMode,
                        JsValueRef *result) {
  if (isStrictMode) {
    // do not append new line so the line numbers on error stack are correct
    std::wstring useStrictTag(L"'use strict'; ");
    return JsParseScript(useStrictTag.append(script).c_str(), sourceContext,
                  sourceUrl, result);
  } else {
    return JsParseScript(script, sourceContext, sourceUrl, result);
  }
}

void Unimplemented(const char * message) {
  fprintf(stderr, "FATAL ERROR: '%s' unimplemented", message);
  __debugbreak();
  abort();
}

void Fatal(const char * format, ...) {
  bool hasException;
  JsErrorCode errorCode;
  JsValueRef exceptionRef;
  JsValueRef stackRef;
  JsValueRef strErrorRef;
  size_t stringLength;
  const wchar_t* strError;

  va_list args;
  va_start(args, format);
  fprintf(stderr, "FATAL ERROR: ");
  vfprintf(stderr, format, args);
  va_end(args);

  errorCode = JsHasException(&hasException);
  if (!hasException || errorCode != JsNoError) {
    if (errorCode != JsNoError)
      fprintf(stderr, "\nImportant: While trying to check Javascript "
        "exception, JsHasException has also failed.\n");
    else
      fprintf(stderr, "\nImportant: This didn't happen because of an "
        "uncaught Javascript exception.\n");
  } else if (JsGetAndClearException(&exceptionRef) == JsNoError &&
           GetProperty(exceptionRef, L"stack", &stackRef) == JsNoError &&
           JsConvertValueToString(stackRef, &strErrorRef) == JsNoError  &&
           JsStringToPointer(strErrorRef, &strError,
                             &stringLength) == JsNoError) {
    fwprintf(stderr, L"\n%s\n", strError);
  }

#ifdef DEBUG
  __debugbreak();
#endif

  abort();
}


JsValueRef CALLBACK CollectGarbage(
  JsValueRef callee,
  bool isConstructCall,
  JsValueRef *arguments,
  unsigned short argumentCount,
  void *callbackState) {
  JsCollectGarbage(IsolateShim::GetCurrent()->GetRuntimeHandle());
  return jsrt::GetUndefined();
}

}  // namespace jsrt

