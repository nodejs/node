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
  // Replace the internal table with a new empty table.
  %SetInitialize(this);
}


// -------------------------------------------------------------------

function SetUpSet() {
  %CheckIsBootstrapping();

  %SetCode($Set, SetConstructor);
  %FunctionSetPrototype($Set, new $Object());
  %SetProperty($Set.prototype, "constructor", $Set, DONT_ENUM);

  // Set up the non-enumerable functions on the Set prototype object.
  InstallGetter($Set.prototype, "size", SetGetSize);
  InstallFunctions($Set.prototype, DONT_ENUM, $Array(
    "add", SetAdd,
    "has", SetHas,
    "delete", SetDelete,
    "clear", SetClear
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
  // Replace the internal table with a new empty table.
  %MapInitialize(this);
}


// -------------------------------------------------------------------

function SetUpMap() {
  %CheckIsBootstrapping();

  %SetCode($Map, MapConstructor);
  %FunctionSetPrototype($Map, new $Object());
  %SetProperty($Map.prototype, "constructor", $Map, DONT_ENUM);

  // Set up the non-enumerable functions on the Map prototype object.
  InstallGetter($Map.prototype, "size", MapGetSize);
  InstallFunctions($Map.prototype, DONT_ENUM, $Array(
    "get", MapGet,
    "set", MapSet,
    "has", MapHas,
    "delete", MapDelete,
    "clear", MapClear
  ));
}

SetUpMap();
