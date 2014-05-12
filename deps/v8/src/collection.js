// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

var $Set = global.Set;
var $Map = global.Map;

// Global sentinel to be used instead of undefined keys, which are not
// supported internally but required for Harmony sets and maps.
var undefined_sentinel = {};


// Map and Set uses SameValueZero which means that +0 and -0 should be treated
// as the same value.
function NormalizeKey(key) {
  if (IS_UNDEFINED(key)) {
    return undefined_sentinel;
  }

  if (key === 0) {
    return 0;
  }

  return key;
}


// -------------------------------------------------------------------
// Harmony Set

function SetConstructor() {
  if (%_IsConstructCall()) {
    %SetInitialize(this);
  } else {
    throw MakeTypeError('constructor_not_function', ['Set']);
  }
}


function SetAdd(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.add', this]);
  }
  return %SetAdd(this, NormalizeKey(key));
}


function SetHas(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.has', this]);
  }
  return %SetHas(this, NormalizeKey(key));
}


function SetDelete(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.delete', this]);
  }
  key = NormalizeKey(key);
  if (%SetHas(this, key)) {
    %SetDelete(this, key);
    return true;
  } else {
    return false;
  }
}


function SetGetSize() {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.size', this]);
  }
  return %SetGetSize(this);
}


function SetClear() {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.clear', this]);
  }
  %SetClear(this);
}


function SetForEach(f, receiver) {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.forEach', this]);
  }

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [f]);
  }

  var iterator = %SetCreateIterator(this, ITERATOR_KIND_VALUES);
  var entry;
  try {
    while (!(entry = %SetIteratorNext(iterator)).done) {
      %_CallFunction(receiver, entry.value, entry.value, this, f);
    }
  } finally {
    %SetIteratorClose(iterator);
  }
}


// -------------------------------------------------------------------

function SetUpSet() {
  %CheckIsBootstrapping();

  %SetCode($Set, SetConstructor);
  %FunctionSetPrototype($Set, new $Object());
  %SetProperty($Set.prototype, "constructor", $Set, DONT_ENUM);

  %FunctionSetLength(SetForEach, 1);

  // Set up the non-enumerable functions on the Set prototype object.
  InstallGetter($Set.prototype, "size", SetGetSize);
  InstallFunctions($Set.prototype, DONT_ENUM, $Array(
    "add", SetAdd,
    "has", SetHas,
    "delete", SetDelete,
    "clear", SetClear,
    "forEach", SetForEach
  ));
}

SetUpSet();


// -------------------------------------------------------------------
// Harmony Map

function MapConstructor() {
  if (%_IsConstructCall()) {
    %MapInitialize(this);
  } else {
    throw MakeTypeError('constructor_not_function', ['Map']);
  }
}


function MapGet(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.get', this]);
  }
  return %MapGet(this, NormalizeKey(key));
}


function MapSet(key, value) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.set', this]);
  }
  return %MapSet(this, NormalizeKey(key), value);
}


function MapHas(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.has', this]);
  }
  return %MapHas(this, NormalizeKey(key));
}


function MapDelete(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.delete', this]);
  }
  return %MapDelete(this, NormalizeKey(key));
}


function MapGetSize() {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.size', this]);
  }
  return %MapGetSize(this);
}


function MapClear() {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.clear', this]);
  }
  %MapClear(this);
}


function MapForEach(f, receiver) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.forEach', this]);
  }

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [f]);
  }

  var iterator = %MapCreateIterator(this, ITERATOR_KIND_ENTRIES);
  var entry;
  try {
    while (!(entry = %MapIteratorNext(iterator)).done) {
      %_CallFunction(receiver, entry.value[1], entry.value[0], this, f);
    }
  } finally {
    %MapIteratorClose(iterator);
  }
}


// -------------------------------------------------------------------

function SetUpMap() {
  %CheckIsBootstrapping();

  %SetCode($Map, MapConstructor);
  %FunctionSetPrototype($Map, new $Object());
  %SetProperty($Map.prototype, "constructor", $Map, DONT_ENUM);

  %FunctionSetLength(MapForEach, 1);

  // Set up the non-enumerable functions on the Map prototype object.
  InstallGetter($Map.prototype, "size", MapGetSize);
  InstallFunctions($Map.prototype, DONT_ENUM, $Array(
    "get", MapGet,
    "set", MapSet,
    "has", MapHas,
    "delete", MapDelete,
    "clear", MapClear,
    "forEach", MapForEach
  ));
}

SetUpMap();
