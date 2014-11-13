// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

var $Set = global.Set;
var $Map = global.Map;


// -------------------------------------------------------------------
// Harmony Set

function SetConstructor(iterable) {
  if (!%_IsConstructCall()) {
    throw MakeTypeError('constructor_not_function', ['Set']);
  }

  var iter, adder;

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    iter = GetIterator(ToObject(iterable));
    adder = this.add;
    if (!IS_SPEC_FUNCTION(adder)) {
      throw MakeTypeError('property_not_function', ['add', this]);
    }
  }

  %SetInitialize(this);

  if (IS_UNDEFINED(iter)) return;

  var next, done;
  while (!(next = iter.next()).done) {
    if (!IS_SPEC_OBJECT(next)) {
      throw MakeTypeError('iterator_result_not_an_object', [next]);
    }
    %_CallFunction(this, next.value, adder);
  }
}


function SetAddJS(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.add', this]);
  }
  // Normalize -0 to +0 as required by the spec.
  // Even though we use SameValueZero as the comparison for the keys we don't
  // want to ever store -0 as the key since the key is directly exposed when
  // doing iteration.
  if (key === 0) {
    key = 0;
  }
  return %SetAdd(this, key);
}


function SetHasJS(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.has', this]);
  }
  return %SetHas(this, key);
}


function SetDeleteJS(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.delete', this]);
  }
  return %SetDelete(this, key);
}


function SetGetSizeJS() {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.size', this]);
  }
  return %SetGetSize(this);
}


function SetClearJS() {
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
  var needs_wrapper = false;
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else {
    needs_wrapper = SHOULD_CREATE_WRAPPER(f, receiver);
  }

  var iterator = new SetIterator(this, ITERATOR_KIND_VALUES);
  var key;
  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  var value_array = [UNDEFINED];
  while (%SetIteratorNext(iterator, value_array)) {
    if (stepping) %DebugPrepareStepInIfStepping(f);
    key = value_array[0];
    var new_receiver = needs_wrapper ? ToObject(receiver) : receiver;
    %_CallFunction(new_receiver, key, key, this, f);
  }
}


// -------------------------------------------------------------------

function SetUpSet() {
  %CheckIsBootstrapping();

  %SetCode($Set, SetConstructor);
  %FunctionSetPrototype($Set, new $Object());
  %AddNamedProperty($Set.prototype, "constructor", $Set, DONT_ENUM);
  %AddNamedProperty(
      $Set.prototype, symbolToStringTag, "Set", DONT_ENUM | READ_ONLY);

  %FunctionSetLength(SetForEach, 1);

  // Set up the non-enumerable functions on the Set prototype object.
  InstallGetter($Set.prototype, "size", SetGetSizeJS);
  InstallFunctions($Set.prototype, DONT_ENUM, $Array(
    "add", SetAddJS,
    "has", SetHasJS,
    "delete", SetDeleteJS,
    "clear", SetClearJS,
    "forEach", SetForEach
  ));
}

SetUpSet();


// -------------------------------------------------------------------
// Harmony Map

function MapConstructor(iterable) {
  if (!%_IsConstructCall()) {
    throw MakeTypeError('constructor_not_function', ['Map']);
  }

  var iter, adder;

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    iter = GetIterator(ToObject(iterable));
    adder = this.set;
    if (!IS_SPEC_FUNCTION(adder)) {
      throw MakeTypeError('property_not_function', ['set', this]);
    }
  }

  %MapInitialize(this);

  if (IS_UNDEFINED(iter)) return;

  var next, done, nextItem;
  while (!(next = iter.next()).done) {
    if (!IS_SPEC_OBJECT(next)) {
      throw MakeTypeError('iterator_result_not_an_object', [next]);
    }
    nextItem = next.value;
    if (!IS_SPEC_OBJECT(nextItem)) {
      throw MakeTypeError('iterator_value_not_an_object', [nextItem]);
    }
    %_CallFunction(this, nextItem[0], nextItem[1], adder);
  }
}


function MapGetJS(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.get', this]);
  }
  return %MapGet(this, key);
}


function MapSetJS(key, value) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.set', this]);
  }
  // Normalize -0 to +0 as required by the spec.
  // Even though we use SameValueZero as the comparison for the keys we don't
  // want to ever store -0 as the key since the key is directly exposed when
  // doing iteration.
  if (key === 0) {
    key = 0;
  }
  return %MapSet(this, key, value);
}


function MapHasJS(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.has', this]);
  }
  return %MapHas(this, key);
}


function MapDeleteJS(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.delete', this]);
  }
  return %MapDelete(this, key);
}


function MapGetSizeJS() {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.size', this]);
  }
  return %MapGetSize(this);
}


function MapClearJS() {
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
  var needs_wrapper = false;
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else {
    needs_wrapper = SHOULD_CREATE_WRAPPER(f, receiver);
  }

  var iterator = new MapIterator(this, ITERATOR_KIND_ENTRIES);
  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  var value_array = [UNDEFINED, UNDEFINED];
  while (%MapIteratorNext(iterator, value_array)) {
    if (stepping) %DebugPrepareStepInIfStepping(f);
    var new_receiver = needs_wrapper ? ToObject(receiver) : receiver;
    %_CallFunction(new_receiver, value_array[1], value_array[0], this, f);
  }
}


// -------------------------------------------------------------------

function SetUpMap() {
  %CheckIsBootstrapping();

  %SetCode($Map, MapConstructor);
  %FunctionSetPrototype($Map, new $Object());
  %AddNamedProperty($Map.prototype, "constructor", $Map, DONT_ENUM);
  %AddNamedProperty(
      $Map.prototype, symbolToStringTag, "Map", DONT_ENUM | READ_ONLY);

  %FunctionSetLength(MapForEach, 1);

  // Set up the non-enumerable functions on the Map prototype object.
  InstallGetter($Map.prototype, "size", MapGetSizeJS);
  InstallFunctions($Map.prototype, DONT_ENUM, $Array(
    "get", MapGetJS,
    "set", MapSetJS,
    "has", MapHasJS,
    "delete", MapDeleteJS,
    "clear", MapClearJS,
    "forEach", MapForEach
  ));
}

SetUpMap();
