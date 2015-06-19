// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, shared, exports) {

"use strict";

%CheckIsBootstrapping();

var GlobalObject = global.Object;
var GlobalWeakMap = global.WeakMap;
var GlobalWeakSet = global.WeakSet;

// -------------------------------------------------------------------
// Harmony WeakMap

function WeakMapConstructor(iterable) {
  if (!%_IsConstructCall()) {
    throw MakeTypeError(kConstructorNotFunction, "WeakMap");
  }

  %WeakCollectionInitialize(this);

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    var adder = this.set;
    if (!IS_SPEC_FUNCTION(adder)) {
      throw MakeTypeError(kPropertyNotFunction, 'set', this);
    }
    for (var nextItem of iterable) {
      if (!IS_SPEC_OBJECT(nextItem)) {
        throw MakeTypeError(kIteratorValueNotAnObject, nextItem);
      }
      %_CallFunction(this, nextItem[0], nextItem[1], adder);
    }
  }
}


function WeakMapGet(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakMap.prototype.get', this);
  }
  if (!IS_SPEC_OBJECT(key)) return UNDEFINED;
  return %WeakCollectionGet(this, key);
}


function WeakMapSet(key, value) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakMap.prototype.set', this);
  }
  if (!IS_SPEC_OBJECT(key)) {
    throw %MakeTypeError('invalid_weakmap_key', [this, key]);
  }
  return %WeakCollectionSet(this, key, value);
}


function WeakMapHas(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakMap.prototype.has', this);
  }
  if (!IS_SPEC_OBJECT(key)) return false;
  return %WeakCollectionHas(this, key);
}


function WeakMapDelete(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakMap.prototype.delete', this);
  }
  if (!IS_SPEC_OBJECT(key)) return false;
  return %WeakCollectionDelete(this, key);
}


// -------------------------------------------------------------------

%SetCode(GlobalWeakMap, WeakMapConstructor);
%FunctionSetLength(GlobalWeakMap, 0);
%FunctionSetPrototype(GlobalWeakMap, new GlobalObject());
%AddNamedProperty(GlobalWeakMap.prototype, "constructor", GlobalWeakMap,
                  DONT_ENUM);
%AddNamedProperty(GlobalWeakMap.prototype, symbolToStringTag, "WeakMap",
                  DONT_ENUM | READ_ONLY);

// Set up the non-enumerable functions on the WeakMap prototype object.
$installFunctions(GlobalWeakMap.prototype, DONT_ENUM, [
  "get", WeakMapGet,
  "set", WeakMapSet,
  "has", WeakMapHas,
  "delete", WeakMapDelete
]);

// -------------------------------------------------------------------
// Harmony WeakSet

function WeakSetConstructor(iterable) {
  if (!%_IsConstructCall()) {
    throw MakeTypeError(kConstructorNotFunction, "WeakSet");
  }

  %WeakCollectionInitialize(this);

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    var adder = this.add;
    if (!IS_SPEC_FUNCTION(adder)) {
      throw MakeTypeError(kPropertyNotFunction, 'add', this);
    }
    for (var value of iterable) {
      %_CallFunction(this, value, adder);
    }
  }
}


function WeakSetAdd(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakSet.prototype.add', this);
  }
  if (!IS_SPEC_OBJECT(value)) {
    throw %MakeTypeError('invalid_weakset_value', [this, value]);
  }
  return %WeakCollectionSet(this, value, true);
}


function WeakSetHas(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakSet.prototype.has', this);
  }
  if (!IS_SPEC_OBJECT(value)) return false;
  return %WeakCollectionHas(this, value);
}


function WeakSetDelete(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakSet.prototype.delete', this);
  }
  if (!IS_SPEC_OBJECT(value)) return false;
  return %WeakCollectionDelete(this, value);
}


// -------------------------------------------------------------------

%SetCode(GlobalWeakSet, WeakSetConstructor);
%FunctionSetLength(GlobalWeakSet, 0);
%FunctionSetPrototype(GlobalWeakSet, new GlobalObject());
%AddNamedProperty(GlobalWeakSet.prototype, "constructor", GlobalWeakSet,
                 DONT_ENUM);
%AddNamedProperty(GlobalWeakSet.prototype, symbolToStringTag, "WeakSet",
                  DONT_ENUM | READ_ONLY);

// Set up the non-enumerable functions on the WeakSet prototype object.
$installFunctions(GlobalWeakSet.prototype, DONT_ENUM, [
  "add", WeakSetAdd,
  "has", WeakSetHas,
  "delete", WeakSetDelete
]);

})
