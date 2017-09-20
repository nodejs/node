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
var GlobalWeakMap = global.WeakMap;
var GlobalWeakSet = global.WeakSet;

utils.Import(function(from) {
  GetExistingHash = from.GetExistingHash;
  GetHash = from.GetHash;
});

// -------------------------------------------------------------------
// Harmony WeakMap

function WeakMapConstructor(iterable) {
  if (IS_UNDEFINED(new.target)) {
    throw %make_type_error(kConstructorNotFunction, "WeakMap");
  }

  %WeakCollectionInitialize(this);

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    var adder = this.set;
    if (!IS_CALLABLE(adder)) {
      throw %make_type_error(kPropertyNotFunction, adder, 'set', this);
    }
    for (var nextItem of iterable) {
      if (!IS_RECEIVER(nextItem)) {
        throw %make_type_error(kIteratorValueNotAnObject, nextItem);
      }
      %_Call(adder, this, nextItem[0], nextItem[1]);
    }
  }
}


// Set up the non-enumerable functions on the WeakMap prototype object.
DEFINE_METHODS(
  GlobalWeakMap.prototype,
  {
    set(key, value) {
      if (!IS_WEAKMAP(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver,
                            'WeakMap.prototype.set', this);
      }
      if (!IS_RECEIVER(key)) throw %make_type_error(kInvalidWeakMapKey);
      return %WeakCollectionSet(this, key, value, GetHash(key));
    }

    delete(key) {
      if (!IS_WEAKMAP(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver,
                            'WeakMap.prototype.delete', this);
      }
      if (!IS_RECEIVER(key)) return false;
      var hash = GetExistingHash(key);
      if (IS_UNDEFINED(hash)) return false;
      return %WeakCollectionDelete(this, key, hash);
    }
  }
);

// -------------------------------------------------------------------

%SetCode(GlobalWeakMap, WeakMapConstructor);
%FunctionSetLength(GlobalWeakMap, 0);

// -------------------------------------------------------------------
// Harmony WeakSet

function WeakSetConstructor(iterable) {
  if (IS_UNDEFINED(new.target)) {
    throw %make_type_error(kConstructorNotFunction, "WeakSet");
  }

  %WeakCollectionInitialize(this);

  if (!IS_NULL_OR_UNDEFINED(iterable)) {
    var adder = this.add;
    if (!IS_CALLABLE(adder)) {
      throw %make_type_error(kPropertyNotFunction, adder, 'add', this);
    }
    for (var value of iterable) {
      %_Call(adder, this, value);
    }
  }
}


// Set up the non-enumerable functions on the WeakSet prototype object.
DEFINE_METHODS(
  GlobalWeakSet.prototype,
  {
    add(value) {
      if (!IS_WEAKSET(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver,
                            'WeakSet.prototype.add', this);
      }
      if (!IS_RECEIVER(value)) throw %make_type_error(kInvalidWeakSetValue);
      return %WeakCollectionSet(this, value, true, GetHash(value));
    }

    delete(value) {
      if (!IS_WEAKSET(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver,
                            'WeakSet.prototype.delete', this);
      }
      if (!IS_RECEIVER(value)) return false;
      var hash = GetExistingHash(value);
      if (IS_UNDEFINED(hash)) return false;
      return %WeakCollectionDelete(this, value, hash);
    }
  }
);

// -------------------------------------------------------------------

%SetCode(GlobalWeakSet, WeakSetConstructor);
%FunctionSetLength(GlobalWeakSet, 0);

})
