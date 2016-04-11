// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {
"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalMap = global.Map;
var GlobalObject = global.Object;
var GlobalSet = global.Set;
var hashCodeSymbol = utils.ImportNow("hash_code_symbol");
var IntRandom;
var MakeTypeError;
var MapIterator;
var NumberIsNaN;
var SetIterator;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  IntRandom = from.IntRandom;
  MakeTypeError = from.MakeTypeError;
  MapIterator = from.MapIterator;
  NumberIsNaN = from.NumberIsNaN;
  SetIterator = from.SetIterator;
});

// -------------------------------------------------------------------

function HashToEntry(table, hash, numBuckets) {
  var bucket = ORDERED_HASH_TABLE_HASH_TO_BUCKET(hash, numBuckets);
  return ORDERED_HASH_TABLE_BUCKET_AT(table, bucket);
}
%SetForceInlineFlag(HashToEntry);


function SetFindEntry(table, numBuckets, key, hash) {
  var entry = HashToEntry(table, hash, numBuckets);
  if (entry === NOT_FOUND) return entry;
  var candidate = ORDERED_HASH_SET_KEY_AT(table, entry, numBuckets);
  if (key === candidate) return entry;
  var keyIsNaN = NumberIsNaN(key);
  while (true) {
    if (keyIsNaN && NumberIsNaN(candidate)) {
      return entry;
    }
    entry = ORDERED_HASH_SET_CHAIN_AT(table, entry, numBuckets);
    if (entry === NOT_FOUND) return entry;
    candidate = ORDERED_HASH_SET_KEY_AT(table, entry, numBuckets);
    if (key === candidate) return entry;
  }
  return NOT_FOUND;
}
%SetForceInlineFlag(SetFindEntry);


function MapFindEntry(table, numBuckets, key, hash) {
  var entry = HashToEntry(table, hash, numBuckets);
  if (entry === NOT_FOUND) return entry;
  var candidate = ORDERED_HASH_MAP_KEY_AT(table, entry, numBuckets);
  if (key === candidate) return entry;
  var keyIsNaN = NumberIsNaN(key);
  while (true) {
    if (keyIsNaN && NumberIsNaN(candidate)) {
      return entry;
    }
    entry = ORDERED_HASH_MAP_CHAIN_AT(table, entry, numBuckets);
    if (entry === NOT_FOUND) return entry;
    candidate = ORDERED_HASH_MAP_KEY_AT(table, entry, numBuckets);
    if (key === candidate) return entry;
  }
  return NOT_FOUND;
}
%SetForceInlineFlag(MapFindEntry);


function ComputeIntegerHash(key, seed) {
  var hash = key;
  hash = hash ^ seed;
  hash = ~hash + (hash << 15);  // hash = (hash << 15) - hash - 1;
  hash = hash ^ (hash >>> 12);
  hash = hash + (hash << 2);
  hash = hash ^ (hash >>> 4);
  hash = (hash * 2057) | 0;  // hash = (hash + (hash << 3)) + (hash << 11);
  hash = hash ^ (hash >>> 16);
  return hash & 0x3fffffff;
}
%SetForceInlineFlag(ComputeIntegerHash);

function GetExistingHash(key) {
  if (%_IsSmi(key)) {
    return ComputeIntegerHash(key, 0);
  }
  if (IS_STRING(key)) {
    var field = %_StringGetRawHashField(key);
    if ((field & 1 /* Name::kHashNotComputedMask */) === 0) {
      return field >>> 2 /* Name::kHashShift */;
    }
  } else if (IS_RECEIVER(key) && !IS_PROXY(key) && !IS_GLOBAL(key)) {
    var hash = GET_PRIVATE(key, hashCodeSymbol);
    return hash;
  }
  return %GenericHash(key);
}
%SetForceInlineFlag(GetExistingHash);


function GetHash(key) {
  var hash = GetExistingHash(key);
  if (IS_UNDEFINED(hash)) {
    hash = IntRandom() | 0;
    if (hash === 0) hash = 1;
    SET_PRIVATE(key, hashCodeSymbol, hash);
  }
  return hash;
}
%SetForceInlineFlag(GetHash);


// -------------------------------------------------------------------
// Harmony Set

function SetConstructor(iterable) {
  if (IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kConstructorNotFunction, "Set");
  }

  %_SetInitialize(this);

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


function SetAdd(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver, 'Set.prototype.add', this);
  }
  // Normalize -0 to +0 as required by the spec.
  // Even though we use SameValueZero as the comparison for the keys we don't
  // want to ever store -0 as the key since the key is directly exposed when
  // doing iteration.
  if (key === 0) {
    key = 0;
  }
  var table = %_JSCollectionGetTable(this);
  var numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
  var hash = GetHash(key);
  if (SetFindEntry(table, numBuckets, key, hash) !== NOT_FOUND) return this;

  var nof = ORDERED_HASH_TABLE_ELEMENT_COUNT(table);
  var nod = ORDERED_HASH_TABLE_DELETED_COUNT(table);
  var capacity = numBuckets << 1;
  if ((nof + nod) >= capacity) {
    // Need to grow, bail out to runtime.
    %SetGrow(this);
    // Re-load state from the grown backing store.
    table = %_JSCollectionGetTable(this);
    numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
    nof = ORDERED_HASH_TABLE_ELEMENT_COUNT(table);
    nod = ORDERED_HASH_TABLE_DELETED_COUNT(table);
  }
  var entry = nof + nod;
  var index = ORDERED_HASH_SET_ENTRY_TO_INDEX(entry, numBuckets);
  var bucket = ORDERED_HASH_TABLE_HASH_TO_BUCKET(hash, numBuckets);
  var chainEntry = ORDERED_HASH_TABLE_BUCKET_AT(table, bucket);
  ORDERED_HASH_TABLE_SET_BUCKET_AT(table, bucket, entry);
  ORDERED_HASH_TABLE_SET_ELEMENT_COUNT(table, nof + 1);
  FIXED_ARRAY_SET(table, index, key);
  FIXED_ARRAY_SET_SMI(table, index + 1, chainEntry);
  return this;
}


function SetHas(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver, 'Set.prototype.has', this);
  }
  var table = %_JSCollectionGetTable(this);
  var numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
  var hash = GetExistingHash(key);
  if (IS_UNDEFINED(hash)) return false;
  return SetFindEntry(table, numBuckets, key, hash) !== NOT_FOUND;
}


function SetDelete(key) {
  if (!IS_SET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Set.prototype.delete', this);
  }
  var table = %_JSCollectionGetTable(this);
  var numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
  var hash = GetExistingHash(key);
  if (IS_UNDEFINED(hash)) return false;
  var entry = SetFindEntry(table, numBuckets, key, hash);
  if (entry === NOT_FOUND) return false;

  var nof = ORDERED_HASH_TABLE_ELEMENT_COUNT(table) - 1;
  var nod = ORDERED_HASH_TABLE_DELETED_COUNT(table) + 1;
  var index = ORDERED_HASH_SET_ENTRY_TO_INDEX(entry, numBuckets);
  FIXED_ARRAY_SET(table, index, %_TheHole());
  ORDERED_HASH_TABLE_SET_ELEMENT_COUNT(table, nof);
  ORDERED_HASH_TABLE_SET_DELETED_COUNT(table, nod);
  if (nof < (numBuckets >>> 1)) %SetShrink(this);
  return true;
}


function SetGetSize() {
  if (!IS_SET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Set.prototype.size', this);
  }
  var table = %_JSCollectionGetTable(this);
  return ORDERED_HASH_TABLE_ELEMENT_COUNT(table);
}


function SetClearJS() {
  if (!IS_SET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Set.prototype.clear', this);
  }
  %_SetClear(this);
}


function SetForEach(f, receiver) {
  if (!IS_SET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Set.prototype.forEach', this);
  }

  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);

  var iterator = new SetIterator(this, ITERATOR_KIND_VALUES);
  var key;
  var value_array = [UNDEFINED];
  while (%SetIteratorNext(iterator, value_array)) {
    key = value_array[0];
    %_Call(f, receiver, key, key, this);
  }
}

// -------------------------------------------------------------------

%SetCode(GlobalSet, SetConstructor);
%FunctionSetLength(GlobalSet, 0);
%FunctionSetPrototype(GlobalSet, new GlobalObject());
%AddNamedProperty(GlobalSet.prototype, "constructor", GlobalSet, DONT_ENUM);
%AddNamedProperty(GlobalSet.prototype, toStringTagSymbol, "Set",
                  DONT_ENUM | READ_ONLY);

%FunctionSetLength(SetForEach, 1);

// Set up the non-enumerable functions on the Set prototype object.
utils.InstallGetter(GlobalSet.prototype, "size", SetGetSize);
utils.InstallFunctions(GlobalSet.prototype, DONT_ENUM, [
  "add", SetAdd,
  "has", SetHas,
  "delete", SetDelete,
  "clear", SetClearJS,
  "forEach", SetForEach
]);


// -------------------------------------------------------------------
// Harmony Map

function MapConstructor(iterable) {
  if (IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kConstructorNotFunction, "Map");
  }

  %_MapInitialize(this);

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


function MapGet(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.get', this);
  }
  var table = %_JSCollectionGetTable(this);
  var numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
  var hash = GetExistingHash(key);
  if (IS_UNDEFINED(hash)) return UNDEFINED;
  var entry = MapFindEntry(table, numBuckets, key, hash);
  if (entry === NOT_FOUND) return UNDEFINED;
  return ORDERED_HASH_MAP_VALUE_AT(table, entry, numBuckets);
}


function MapSet(key, value) {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.set', this);
  }
  // Normalize -0 to +0 as required by the spec.
  // Even though we use SameValueZero as the comparison for the keys we don't
  // want to ever store -0 as the key since the key is directly exposed when
  // doing iteration.
  if (key === 0) {
    key = 0;
  }

  var table = %_JSCollectionGetTable(this);
  var numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
  var hash = GetHash(key);
  var entry = MapFindEntry(table, numBuckets, key, hash);
  if (entry !== NOT_FOUND) {
    var existingIndex = ORDERED_HASH_MAP_ENTRY_TO_INDEX(entry, numBuckets);
    FIXED_ARRAY_SET(table, existingIndex + 1, value);
    return this;
  }

  var nof = ORDERED_HASH_TABLE_ELEMENT_COUNT(table);
  var nod = ORDERED_HASH_TABLE_DELETED_COUNT(table);
  var capacity = numBuckets << 1;
  if ((nof + nod) >= capacity) {
    // Need to grow, bail out to runtime.
    %MapGrow(this);
    // Re-load state from the grown backing store.
    table = %_JSCollectionGetTable(this);
    numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
    nof = ORDERED_HASH_TABLE_ELEMENT_COUNT(table);
    nod = ORDERED_HASH_TABLE_DELETED_COUNT(table);
  }
  entry = nof + nod;
  var index = ORDERED_HASH_MAP_ENTRY_TO_INDEX(entry, numBuckets);
  var bucket = ORDERED_HASH_TABLE_HASH_TO_BUCKET(hash, numBuckets);
  var chainEntry = ORDERED_HASH_TABLE_BUCKET_AT(table, bucket);
  ORDERED_HASH_TABLE_SET_BUCKET_AT(table, bucket, entry);
  ORDERED_HASH_TABLE_SET_ELEMENT_COUNT(table, nof + 1);
  FIXED_ARRAY_SET(table, index, key);
  FIXED_ARRAY_SET(table, index + 1, value);
  FIXED_ARRAY_SET(table, index + 2, chainEntry);
  return this;
}


function MapHas(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.has', this);
  }
  var table = %_JSCollectionGetTable(this);
  var numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
  var hash = GetHash(key);
  return MapFindEntry(table, numBuckets, key, hash) !== NOT_FOUND;
}


function MapDelete(key) {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.delete', this);
  }
  var table = %_JSCollectionGetTable(this);
  var numBuckets = ORDERED_HASH_TABLE_BUCKET_COUNT(table);
  var hash = GetHash(key);
  var entry = MapFindEntry(table, numBuckets, key, hash);
  if (entry === NOT_FOUND) return false;

  var nof = ORDERED_HASH_TABLE_ELEMENT_COUNT(table) - 1;
  var nod = ORDERED_HASH_TABLE_DELETED_COUNT(table) + 1;
  var index = ORDERED_HASH_MAP_ENTRY_TO_INDEX(entry, numBuckets);
  FIXED_ARRAY_SET(table, index, %_TheHole());
  FIXED_ARRAY_SET(table, index + 1, %_TheHole());
  ORDERED_HASH_TABLE_SET_ELEMENT_COUNT(table, nof);
  ORDERED_HASH_TABLE_SET_DELETED_COUNT(table, nod);
  if (nof < (numBuckets >>> 1)) %MapShrink(this);
  return true;
}


function MapGetSize() {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.size', this);
  }
  var table = %_JSCollectionGetTable(this);
  return ORDERED_HASH_TABLE_ELEMENT_COUNT(table);
}


function MapClearJS() {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.clear', this);
  }
  %_MapClear(this);
}


function MapForEach(f, receiver) {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.forEach', this);
  }

  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);

  var iterator = new MapIterator(this, ITERATOR_KIND_ENTRIES);
  var value_array = [UNDEFINED, UNDEFINED];
  while (%MapIteratorNext(iterator, value_array)) {
    %_Call(f, receiver, value_array[1], value_array[0], this);
  }
}

// -------------------------------------------------------------------

%SetCode(GlobalMap, MapConstructor);
%FunctionSetLength(GlobalMap, 0);
%FunctionSetPrototype(GlobalMap, new GlobalObject());
%AddNamedProperty(GlobalMap.prototype, "constructor", GlobalMap, DONT_ENUM);
%AddNamedProperty(
    GlobalMap.prototype, toStringTagSymbol, "Map", DONT_ENUM | READ_ONLY);

%FunctionSetLength(MapForEach, 1);

// Set up the non-enumerable functions on the Map prototype object.
utils.InstallGetter(GlobalMap.prototype, "size", MapGetSize);
utils.InstallFunctions(GlobalMap.prototype, DONT_ENUM, [
  "get", MapGet,
  "set", MapSet,
  "has", MapHas,
  "delete", MapDelete,
  "clear", MapClearJS,
  "forEach", MapForEach
]);

// -----------------------------------------------------------------------
// Exports

%InstallToContext([
  "map_get", MapGet,
  "map_set", MapSet,
  "map_has", MapHas,
  "map_delete", MapDelete,
  "set_add", SetAdd,
  "set_has", SetHas,
  "set_delete", SetDelete,
]);

utils.Export(function(to) {
  to.GetExistingHash = GetExistingHash;
  to.GetHash = GetHash;
});

})
