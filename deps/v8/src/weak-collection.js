// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

var $WeakMap = global.WeakMap;
var $WeakSet = global.WeakSet;


// -------------------------------------------------------------------
// Harmony WeakMap

function WeakMapConstructor(iterable) {
  if (!%_IsConstructCall()) {
    throw MakeTypeError('constructor_not_function', ['WeakMap']);
  }

  %WeakCollectionInitialize(this);

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    var adder = this.set;
    if (!IS_SPEC_FUNCTION(adder)) {
      throw MakeTypeError('property_not_function', ['set', this]);
    }
    for (var nextItem of iterable) {
      if (!IS_SPEC_OBJECT(nextItem)) {
        throw MakeTypeError('iterator_value_not_an_object', [nextItem]);
      }
      %_CallFunction(this, nextItem[0], nextItem[1], adder);
    }
  }
}


function WeakMapGet(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.get', this]);
  }
  if (!IS_SPEC_OBJECT(key)) return UNDEFINED;
  return %WeakCollectionGet(this, key);
}


function WeakMapSet(key, value) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.set', this]);
  }
  if (!IS_SPEC_OBJECT(key)) {
    throw %MakeTypeError('invalid_weakmap_key', [this, key]);
  }
  return %WeakCollectionSet(this, key, value);
}


function WeakMapHas(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.has', this]);
  }
  if (!IS_SPEC_OBJECT(key)) return false;
  return %WeakCollectionHas(this, key);
}


function WeakMapDelete(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.delete', this]);
  }
  if (!IS_SPEC_OBJECT(key)) return false;
  return %WeakCollectionDelete(this, key);
}


// -------------------------------------------------------------------

function SetUpWeakMap() {
  %CheckIsBootstrapping();

  %SetCode($WeakMap, WeakMapConstructor);
  %FunctionSetPrototype($WeakMap, new $Object());
  %AddNamedProperty($WeakMap.prototype, "constructor", $WeakMap, DONT_ENUM);
  %AddNamedProperty(
      $WeakMap.prototype, symbolToStringTag, "WeakMap", DONT_ENUM | READ_ONLY);

  // Set up the non-enumerable functions on the WeakMap prototype object.
  InstallFunctions($WeakMap.prototype, DONT_ENUM, $Array(
    "get", WeakMapGet,
    "set", WeakMapSet,
    "has", WeakMapHas,
    "delete", WeakMapDelete
  ));
}

SetUpWeakMap();


// -------------------------------------------------------------------
// Harmony WeakSet

function WeakSetConstructor(iterable) {
  if (!%_IsConstructCall()) {
    throw MakeTypeError('constructor_not_function', ['WeakSet']);
  }

  %WeakCollectionInitialize(this);

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    var adder = this.add;
    if (!IS_SPEC_FUNCTION(adder)) {
      throw MakeTypeError('property_not_function', ['add', this]);
    }
    for (var value of iterable) {
      %_CallFunction(this, value, adder);
    }
  }
}


function WeakSetAdd(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakSet.prototype.add', this]);
  }
  if (!IS_SPEC_OBJECT(value)) {
    throw %MakeTypeError('invalid_weakset_value', [this, value]);
  }
  return %WeakCollectionSet(this, value, true);
}


function WeakSetHas(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakSet.prototype.has', this]);
  }
  if (!IS_SPEC_OBJECT(value)) return false;
  return %WeakCollectionHas(this, value);
}


function WeakSetDelete(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakSet.prototype.delete', this]);
  }
  if (!IS_SPEC_OBJECT(value)) return false;
  return %WeakCollectionDelete(this, value);
}


// -------------------------------------------------------------------

function SetUpWeakSet() {
  %CheckIsBootstrapping();

  %SetCode($WeakSet, WeakSetConstructor);
  %FunctionSetPrototype($WeakSet, new $Object());
  %AddNamedProperty($WeakSet.prototype, "constructor", $WeakSet, DONT_ENUM);
  %AddNamedProperty(
      $WeakSet.prototype, symbolToStringTag, "WeakSet", DONT_ENUM | READ_ONLY);

  // Set up the non-enumerable functions on the WeakSet prototype object.
  InstallFunctions($WeakSet.prototype, DONT_ENUM, $Array(
    "add", WeakSetAdd,
    "has", WeakSetHas,
    "delete", WeakSetDelete
  ));
}

SetUpWeakSet();
