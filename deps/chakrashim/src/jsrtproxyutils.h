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

#include "jsrtutils.h"

namespace jsrt {
// Proxy Traps types, please see:
// http://wiki.ecmascript.org/doku.php?id=harmony:direct_proxies
// for more details regarding harmony proxies
enum ProxyTraps {
  ApplyTrap,
  ConstructTrap,
  DefinePropertyTrap,
  DeletePropertyTrap,
  EnumerateTrap,
  GetTrap,
  GetOwnPropertyDescriptorTrap,
  GetPrototypeOfTrap,
  HasTrap,
  IsExtensibleTrap,
  OwnKeysTrap,
  PreventExtensionsTrap,
  SetTrap,
  SetPrototypeOfTrap,
  TrapCount
};

enum CachedPropertyIdRef;
CachedPropertyIdRef GetProxyTrapCachedPropertyIdRef(ProxyTraps trap);

JsErrorCode CreateProxy(
    JsValueRef target,
    const JsNativeFunction config[ProxyTraps::TrapCount],
    JsValueRef *result);

JsErrorCode TryParseUInt32(JsValueRef strRef,
                           bool* isUInt32,
                           unsigned int *uint32Value);

}  // namespace jsrt
