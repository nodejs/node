// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GetExistingHash;
var GetHash;
var GlobalObject = global.Object;
var GlobalWeakMap = global.WeakMap;
var GlobalWeakSet = global.WeakSet;
var MakeTypeError;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  GetExistingHash = from.GetExistingHash;
  GetHash = from.GetHash;
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------
// Harmony WeakMap

function WeakMapConstructor(iterable) {
  if (IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kConstructorNotFunction, "WeakMap");
  }

  %WeakCollectionInitialize(this);

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    var adder = this.set;
    if (!IS_CALLABLE(adder)) {
      throw MakeTypeError(kPropertyNotFunction, adder, 'set', this);
    }
    for (var nextItem of iterable) {
      if (!IS_RECEIVER(nextItem)) {
        throw MakeTypeError(kIteratorValueNotAnObject, nextItem);
      }
      %_Call(adder, this, nextItem[0], nextItem[1]);
    }
  }
}


function WeakMapGet(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakMap.prototype.get', this);
  }
  if (!IS_RECEIVER(key)) return UNDEFINED;
  var hash = GetExistingHash(key);
  if (IS_UNDEFINED(hash)) return UNDEFINED;
  return %WeakCollectionGet(this, key, hash);
}


function WeakMapSet(key, value) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakMap.prototype.set', this);
  }
  if (!IS_RECEIVER(key)) throw MakeTypeError(kInvalidWeakMapKey);
  return %WeakCollectionSet(this, key, value, GetHash(key));
}


function WeakMapHas(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakMap.prototype.has', this);
  }
  if (!IS_RECEIVER(key)) return false;
  var hash = GetExistingHash(key);
  if (IS_UNDEFINED(hash)) return false;
  return %WeakCollectionHas(this, key, hash);
}


function WeakMapDelete(key) {
  if (!IS_WEAKMAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakMap.prototype.delete', this);
  }
  if (!IS_RECEIVER(key)) return false;
  var hash = GetExistingHash(key);
  if (IS_UNDEFINED(hash)) return false;
  return %WeakCollectionDelete(this, key, hash);
}


// -------------------------------------------------------------------

%SetCode(GlobalWeakMap, WeakMapConstructor);
%FunctionSetLength(GlobalWeakMap, 0);
%FunctionSetPrototype(GlobalWeakMap, new GlobalObject());
%AddNamedProperty(GlobalWeakMap.prototype, "constructor", GlobalWeakMap,
                  DONT_ENUM);
%AddNamedProperty(GlobalWeakMap.prototype, toStringTagSymbol, "WeakMap",
                  DONT_ENUM | READ_ONLY);

// Set up the non-enumerable functions on the WeakMap prototype object.
utils.InstallFunctions(GlobalWeakMap.prototype, DONT_ENUM, [
  "get", WeakMapGet,
  "set", WeakMapSet,
  "has", WeakMapHas,
  "delete", WeakMapDelete
]);

// -------------------------------------------------------------------
// Harmony WeakSet

function WeakSetConstructor(iterable) {
  if (IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kConstructorNotFunction, "WeakSet");
  }

  %WeakCollectionInitialize(this);

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    var adder = this.add;
    if (!IS_CALLABLE(adder)) {
      throw MakeTypeError(kPropertyNotFunction, adder, 'add', this);
    }
    for (var value of iterable) {
      %_Call(adder, this, value);
    }
  }
}


function WeakSetAdd(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakSet.prototype.add', this);
  }
  if (!IS_RECEIVER(value)) throw MakeTypeError(kInvalidWeakSetValue);
  return %WeakCollectionSet(this, value, true, GetHash(value));
}


function WeakSetHas(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakSet.prototype.has', this);
  }
  if (!IS_RECEIVER(value)) return false;
  var hash = GetExistingHash(value);
  if (IS_UNDEFINED(hash)) return false;
  return %WeakCollectionHas(this, value, hash);
}


function WeakSetDelete(value) {
  if (!IS_WEAKSET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'WeakSet.prototype.delete', this);
  }
  if (!IS_RECEIVER(value)) return false;
  var hash = GetExistingHash(value);
  if (IS_UNDEFINED(hash)) return false;
  return %WeakCollectionDelete(this, value, hash);
}


// -------------------------------------------------------------------

%SetCode(GlobalWeakSet, WeakSetConstructor);
%FunctionSetLength(GlobalWeakSet, 0);
%FunctionSetPrototype(GlobalWeakSet, new GlobalObject());
%AddNamedProperty(GlobalWeakSet.prototype, "constructor", GlobalWeakSet,
                 DONT_ENUM);
%AddNamedProperty(GlobalWeakSet.prototype, toStringTagSymbol, "WeakSet",
                  DONT_ENUM | READ_ONLY);

// Set up the non-enumerable functions on the WeakSet prototype object.
utils.InstallFunctions(GlobalWeakSet.prototype, DONT_ENUM, [
  "add", WeakSetAdd,
  "has", WeakSetHas,
  "delete", WeakSetDelete
]);

})
