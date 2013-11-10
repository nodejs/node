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

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Object = global.Object;

var $Proxy = new $Object();

// -------------------------------------------------------------------

function ProxyCreate(handler, proto) {
  if (!IS_SPEC_OBJECT(handler))
    throw MakeTypeError("handler_non_object", ["create"])
  if (IS_UNDEFINED(proto))
    proto = null
  else if (!(IS_SPEC_OBJECT(proto) || IS_NULL(proto)))
    throw MakeTypeError("proto_non_object", ["create"])
  return %CreateJSProxy(handler, proto)
}

function ProxyCreateFunction(handler, callTrap, constructTrap) {
  if (!IS_SPEC_OBJECT(handler))
    throw MakeTypeError("handler_non_object", ["create"])
  if (!IS_SPEC_FUNCTION(callTrap))
    throw MakeTypeError("trap_function_expected", ["createFunction", "call"])
  if (IS_UNDEFINED(constructTrap)) {
    constructTrap = DerivedConstructTrap(callTrap)
  } else if (IS_SPEC_FUNCTION(constructTrap)) {
    // Make sure the trap receives 'undefined' as this.
    var construct = constructTrap
    constructTrap = function() {
      return %Apply(construct, UNDEFINED, arguments, 0, %_ArgumentsLength());
    }
  } else {
    throw MakeTypeError("trap_function_expected",
                        ["createFunction", "construct"])
  }
  return %CreateJSFunctionProxy(
    handler, callTrap, constructTrap, $Function.prototype)
}


// -------------------------------------------------------------------

function SetUpProxy() {
  %CheckIsBootstrapping()

  global.Proxy = $Proxy;

  // Set up non-enumerable properties of the Proxy object.
  InstallFunctions($Proxy, DONT_ENUM, [
    "create", ProxyCreate,
    "createFunction", ProxyCreateFunction
  ])
}

SetUpProxy();


// -------------------------------------------------------------------
// Proxy Builtins

function DerivedConstructTrap(callTrap) {
  return function() {
    var proto = this.prototype
    if (!IS_SPEC_OBJECT(proto)) proto = $Object.prototype
    var obj = { __proto__: proto };
    var result = %Apply(callTrap, obj, arguments, 0, %_ArgumentsLength());
    return IS_SPEC_OBJECT(result) ? result : obj
  }
}

function DelegateCallAndConstruct(callTrap, constructTrap) {
  return function() {
    return %Apply(%_IsConstructCall() ? constructTrap : callTrap,
                  this, arguments, 0, %_ArgumentsLength())
  }
}

function DerivedGetTrap(receiver, name) {
  var desc = this.getPropertyDescriptor(name)
  if (IS_UNDEFINED(desc)) { return desc }
  if ('value' in desc) {
    return desc.value
  } else {
    if (IS_UNDEFINED(desc.get)) { return desc.get }
    // The proposal says: desc.get.call(receiver)
    return %_CallFunction(receiver, desc.get)
  }
}

function DerivedSetTrap(receiver, name, val) {
  var desc = this.getOwnPropertyDescriptor(name)
  if (desc) {
    if ('writable' in desc) {
      if (desc.writable) {
        desc.value = val
        this.defineProperty(name, desc)
        return true
      } else {
        return false
      }
    } else { // accessor
      if (desc.set) {
        // The proposal says: desc.set.call(receiver, val)
        %_CallFunction(receiver, val, desc.set)
        return true
      } else {
        return false
      }
    }
  }
  desc = this.getPropertyDescriptor(name)
  if (desc) {
    if ('writable' in desc) {
      if (desc.writable) {
        // fall through
      } else {
        return false
      }
    } else { // accessor
      if (desc.set) {
        // The proposal says: desc.set.call(receiver, val)
        %_CallFunction(receiver, val, desc.set)
        return true
      } else {
        return false
      }
    }
  }
  this.defineProperty(name, {
    value: val,
    writable: true,
    enumerable: true,
    configurable: true});
  return true;
}

function DerivedHasTrap(name) {
  return !!this.getPropertyDescriptor(name)
}

function DerivedHasOwnTrap(name) {
  return !!this.getOwnPropertyDescriptor(name)
}

function DerivedKeysTrap() {
  var names = this.getOwnPropertyNames()
  var enumerableNames = []
  for (var i = 0, count = 0; i < names.length; ++i) {
    var name = names[i]
    if (IS_SYMBOL(name)) continue
    var desc = this.getOwnPropertyDescriptor(TO_STRING_INLINE(name))
    if (!IS_UNDEFINED(desc) && desc.enumerable) {
      enumerableNames[count++] = names[i]
    }
  }
  return enumerableNames
}

function DerivedEnumerateTrap() {
  var names = this.getPropertyNames()
  var enumerableNames = []
  for (var i = 0, count = 0; i < names.length; ++i) {
    var name = names[i]
    if (IS_SYMBOL(name)) continue
    var desc = this.getPropertyDescriptor(TO_STRING_INLINE(name))
    if (!IS_UNDEFINED(desc)) {
      if (!desc.configurable) {
        throw MakeTypeError("proxy_prop_not_configurable",
            [this, "getPropertyDescriptor", name])
      }
      if (desc.enumerable) enumerableNames[count++] = names[i]
    }
  }
  return enumerableNames
}

function ProxyEnumerate(proxy) {
  var handler = %GetHandler(proxy)
  if (IS_UNDEFINED(handler.enumerate)) {
    return %Apply(DerivedEnumerateTrap, handler, [], 0, 0)
  } else {
    return ToNameArray(handler.enumerate(), "enumerate", false)
  }
}
