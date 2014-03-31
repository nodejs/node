// Copyright 2012 the V8 project authors. All rights reserved.
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
