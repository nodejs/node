// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $proxyDerivedGetTrap;
var $proxyDerivedHasTrap;
var $proxyDerivedSetTrap;
var $proxyEnumerate;

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// ----------------------------------------------------------------------------
// Imports

var GlobalFunction = global.Function;
var GlobalObject = global.Object;

var ToNameArray;

utils.Import(function(from) {
  ToNameArray = from.ToNameArray;
});

//----------------------------------------------------------------------------

function ProxyCreate(handler, proto) {
  if (!IS_SPEC_OBJECT(handler))
    throw MakeTypeError(kProxyHandlerNonObject, "create")
  if (IS_UNDEFINED(proto))
    proto = null
  else if (!(IS_SPEC_OBJECT(proto) || IS_NULL(proto)))
    throw MakeTypeError(kProxyProtoNonObject)
  return %CreateJSProxy(handler, proto)
}

function ProxyCreateFunction(handler, callTrap, constructTrap) {
  if (!IS_SPEC_OBJECT(handler))
    throw MakeTypeError(kProxyHandlerNonObject, "createFunction")
  if (!IS_SPEC_FUNCTION(callTrap))
    throw MakeTypeError(kProxyTrapFunctionExpected, "call")
  if (IS_UNDEFINED(constructTrap)) {
    constructTrap = DerivedConstructTrap(callTrap)
  } else if (IS_SPEC_FUNCTION(constructTrap)) {
    // Make sure the trap receives 'undefined' as this.
    var construct = constructTrap
    constructTrap = function() {
      return %Apply(construct, UNDEFINED, arguments, 0, %_ArgumentsLength());
    }
  } else {
    throw MakeTypeError(kProxyTrapFunctionExpected, "construct")
  }
  return %CreateJSFunctionProxy(
    handler, callTrap, constructTrap, GlobalFunction.prototype)
}

// -------------------------------------------------------------------
// Proxy Builtins

function DerivedConstructTrap(callTrap) {
  return function() {
    var proto = this.prototype
    if (!IS_SPEC_OBJECT(proto)) proto = GlobalObject.prototype
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
        throw MakeTypeError(kProxyPropNotConfigurable,
                            this, name, "getPropertyDescriptor")
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

//-------------------------------------------------------------------

var Proxy = new GlobalObject();
%AddNamedProperty(global, "Proxy", Proxy, DONT_ENUM);

//Set up non-enumerable properties of the Proxy object.
utils.InstallFunctions(Proxy, DONT_ENUM, [
  "create", ProxyCreate,
  "createFunction", ProxyCreateFunction
])

// -------------------------------------------------------------------
// Exports

$proxyDerivedGetTrap = DerivedGetTrap;
$proxyDerivedHasTrap = DerivedHasTrap;
$proxyDerivedSetTrap = DerivedSetTrap;
$proxyEnumerate = ProxyEnumerate;

utils.Export(function(to) {
  to.ProxyDelegateCallAndConstruct = DelegateCallAndConstruct;
  to.ProxyDerivedHasOwnTrap = DerivedHasOwnTrap;
  to.ProxyDerivedKeysTrap = DerivedKeysTrap;
});

})
