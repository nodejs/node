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
#include <string>
#include <assert.h>

namespace jsrt {

// Trap names:
static CachedPropertyIdRef
const s_proxyTrapCachedPropertyIdRef[ProxyTraps::TrapCount] = {
  CachedPropertyIdRef::apply,
  CachedPropertyIdRef::construct,
  CachedPropertyIdRef::defineProperty,
  CachedPropertyIdRef::deleteProperty,
  CachedPropertyIdRef::enumerate,
  CachedPropertyIdRef::get,
  CachedPropertyIdRef::getOwnPropertyDescriptor,
  CachedPropertyIdRef::getPrototypeOf,
  CachedPropertyIdRef::has,
  CachedPropertyIdRef::isExtensible,
  CachedPropertyIdRef::ownKeys,
  CachedPropertyIdRef::preventExtensions,
  CachedPropertyIdRef::set,
  CachedPropertyIdRef::setPrototypeOf
};

CachedPropertyIdRef GetProxyTrapCachedPropertyIdRef(ProxyTraps trap) {
  return s_proxyTrapCachedPropertyIdRef[trap];
}

JsErrorCode SetPropertyOnTrapConfig(
    ProxyTraps trap, JsNativeFunction callback, JsValueRef configObj) {
  if (callback == nullptr) {
    return JsNoError;
  }

  JsErrorCode error;

  JsValueRef func;
  error = JsCreateFunction(callback, nullptr, &func);
  if (error != JsNoError) {
    return error;
  }

  JsPropertyIdRef prop =
    IsolateShim::GetCurrent()->GetProxyTrapPropertyIdRef(trap);

  error = JsSetProperty(configObj, prop, func, false);
  return error;
}

JsErrorCode CreateProxyTrapConfig(
    const JsNativeFunction proxyConf[ProxyTraps::TrapCount],
    JsValueRef *confObj) {
  JsErrorCode error = JsNoError;

  error = JsCreateObject(confObj);

  if (error != JsNoError) {
    return error;
  }

  // Set the properties of the proxy configuration object according to the given
  // map of proxy traps and function handlers For each proxy trap - set the
  // given handler using the appropriate javascript name
  for (int i = 0; i < ProxyTraps::TrapCount; i++) {
    error = SetPropertyOnTrapConfig((ProxyTraps)i, proxyConf[i], *confObj);
    if (error != JsNoError) {
      return error;
    }
  }

  return JsNoError;
}

JsErrorCode CreateProxy(
    JsValueRef target,
    const JsNativeFunction config[ProxyTraps::TrapCount],
    JsValueRef *result) {
  JsErrorCode error;

  JsValueRef proxyConfigObj;
  error = CreateProxyTrapConfig(config, &proxyConfigObj);
  if (error != JsNoError) {
    return error;
  }

  JsValueRef proxyConstructorRef =
    ContextShim::GetCurrent()->GetProxyConstructor();
  error = ConstructObject(proxyConstructorRef, target, proxyConfigObj, result);
  return error;
}

JsErrorCode TryParseUInt32(
    JsValueRef strRef, bool* isUInt32, unsigned int *uint32Value) {
  JsErrorCode error;

  *isUInt32 = false;

  // check that every character in the str is a digit, and that the string does
  // not start with a zero, unless it is a zero
  const wchar_t *strPtr;
  size_t strLength;

  error = JsStringToPointer(strRef, &strPtr, &strLength);
  if (error != JsNoError) {
    // If strRef is not a string, just return not Uint32
    return error == JsErrorInvalidArgument ? JsNoError : error;
  }

  // empty string
  if (strLength == 0) {
    return JsNoError;
  }

  // deal with the case in which zero is the first letter, in which we will
  // accept it only if the string reperesents zero itself
  if (strPtr[0] == L'0') {
    if (strLength == 1) {
      *uint32Value = 0;
      *isUInt32 = true;
    }

    return JsNoError;
  }

  // iterate over the charecters, allow only digits:
  for (size_t i = 0; i < strLength; i++) {
    if (strPtr[i] < L'0' || strPtr[i] > L'9') {
      return JsNoError;
    }
  }

  // use std:stoull as it the most comprehenisve way to convert string to int
  // there is some performance issue here, so we might optimize this code using
  // the results in here: string to int conversion - naive approach is the
  // fastest:
  // http://tinodidriksen.com/2010/02/16/cpp-convert-string-to-int-speed/

  wchar_t* strEnd;
  unsigned long longVal = std::wcstoul(strPtr, &strEnd, 10);
  if (strEnd != strPtr + strLength) {
    return JsNoError;
  }

  if (longVal == ULONG_MAX) {
    // check if errno is set:
    if (errno == ERANGE) {
      return JsNoError;
    }
  }

  *isUInt32 = true;
  *uint32Value = longVal;
  return JsNoError;
}

}  // namespace jsrt
