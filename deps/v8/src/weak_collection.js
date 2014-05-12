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

function WeakMapConstructor() {
  if (%_IsConstructCall()) {
    %WeakCollectionInitialize(this);
  } else {
    throw MakeTypeError('constructor_not_function', ['WeakMap']);
  }
}


function WeakMapGet(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.get', this]);
  }
  if (!(IS_SPEC_OBJECT(key) || IS_SYMBOL(key))) {
    throw %MakeTypeError('invalid_weakmap_key', [this, key]);
  }
  return %WeakCollectionGet(this, key);
}


function WeakMapSet(key, value) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.set', this]);
  }
  if (!(IS_SPEC_OBJECT(key) || IS_SYMBOL(key))) {
    throw %MakeTypeError('invalid_weakmap_key', [this, key]);
  }
  return %WeakCollectionSet(this, key, value);
}


function WeakMapHas(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.has', this]);
  }
  if (!(IS_SPEC_OBJECT(key) || IS_SYMBOL(key))) {
    throw %MakeTypeError('invalid_weakmap_key', [this, key]);
  }
  return %WeakCollectionHas(this, key);
}


function WeakMapDelete(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.delete', this]);
  }
  if (!(IS_SPEC_OBJECT(key) || IS_SYMBOL(key))) {
    throw %MakeTypeError('invalid_weakmap_key', [this, key]);
  }
  return %WeakCollectionDelete(this, key);
}


function WeakMapClear() {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakMap.prototype.clear', this]);
  }
  // Replace the internal table with a new empty table.
  %WeakCollectionInitialize(this);
}


// -------------------------------------------------------------------

function SetUpWeakMap() {
  %CheckIsBootstrapping();

  %SetCode($WeakMap, WeakMapConstructor);
  %FunctionSetPrototype($WeakMap, new $Object());
  %SetProperty($WeakMap.prototype, "constructor", $WeakMap, DONT_ENUM);

  // Set up the non-enumerable functions on the WeakMap prototype object.
  InstallFunctions($WeakMap.prototype, DONT_ENUM, $Array(
    "get", WeakMapGet,
    "set", WeakMapSet,
    "has", WeakMapHas,
    "delete", WeakMapDelete,
    "clear", WeakMapClear
  ));
}

SetUpWeakMap();


// -------------------------------------------------------------------
// Harmony WeakSet

function WeakSetConstructor() {
  if (%_IsConstructCall()) {
    %WeakCollectionInitialize(this);
  } else {
    throw MakeTypeError('constructor_not_function', ['WeakSet']);
  }
}


function WeakSetAdd(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakSet.prototype.add', this]);
  }
  if (!(IS_SPEC_OBJECT(value) || IS_SYMBOL(value))) {
    throw %MakeTypeError('invalid_weakset_value', [this, value]);
  }
  return %WeakCollectionSet(this, value, true);
}


function WeakSetHas(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakSet.prototype.has', this]);
  }
  if (!(IS_SPEC_OBJECT(value) || IS_SYMBOL(value))) {
    throw %MakeTypeError('invalid_weakset_value', [this, value]);
  }
  return %WeakCollectionHas(this, value);
}


function WeakSetDelete(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakSet.prototype.delete', this]);
  }
  if (!(IS_SPEC_OBJECT(value) || IS_SYMBOL(value))) {
    throw %MakeTypeError('invalid_weakset_value', [this, value]);
  }
  return %WeakCollectionDelete(this, value);
}


function WeakSetClear() {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['WeakSet.prototype.clear', this]);
  }
  // Replace the internal table with a new empty table.
  %WeakCollectionInitialize(this);
}


// -------------------------------------------------------------------

function SetUpWeakSet() {
  %CheckIsBootstrapping();

  %SetCode($WeakSet, WeakSetConstructor);
  %FunctionSetPrototype($WeakSet, new $Object());
  %SetProperty($WeakSet.prototype, "constructor", $WeakSet, DONT_ENUM);

  // Set up the non-enumerable functions on the WeakSet prototype object.
  InstallFunctions($WeakSet.prototype, DONT_ENUM, $Array(
    "add", WeakSetAdd,
    "has", WeakSetHas,
    "delete", WeakSetDelete,
    "clear", WeakSetClear
  ));
}

SetUpWeakSet();
