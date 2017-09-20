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
var MathRandom = global.Math.random;
var MapIterator;
var SetIterator;

utils.Import(function(from) {
  MapIterator = from.MapIterator;
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
  var keyIsNaN = NUMBER_IS_NAN(key);
  while (true) {
    if (keyIsNaN && NUMBER_IS_NAN(candidate)) {
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
  var keyIsNaN = NUMBER_IS_NAN(key);
  while (true) {
    if (keyIsNaN && NUMBER_IS_NAN(candidate)) {
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
    hash = (MathRandom() * 0x40000000) | 0;
    if (hash === 0) hash = 1;
    SET_PRIVATE(key, hashCodeSymbol, hash);
  }
  return hash;
}
%SetForceInlineFlag(GetHash);


// -------------------------------------------------------------------
// Harmony Set

//Set up the non-enumerable functions on the Set prototype object.
DEFINE_METHODS(
  GlobalSet.prototype,
  {
    add(key) {
      if (!IS_SET(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver, 'Set.prototype.add', this);
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

    delete(key) {
      if (!IS_SET(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver,
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
  }
);

// Harmony Map

//Set up the non-enumerable functions on the Map prototype object.
DEFINE_METHODS(
  GlobalMap.prototype,
  {
    set(key, value) {
      if (!IS_MAP(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver,
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

    delete(key) {
      if (!IS_MAP(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver,
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
  }
);

// -----------------------------------------------------------------------
// Exports

%InstallToContext([
  "map_set", GlobalMap.prototype.set,
  "map_delete", GlobalMap.prototype.delete,
  "set_add", GlobalSet.prototype.add,
  "set_delete", GlobalSet.prototype.delete,
]);

utils.Export(function(to) {
  to.GetExistingHash = GetExistingHash;
  to.GetHash = GetHash;
});

})
