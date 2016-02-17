// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalMap = global.Map;
var GlobalSet = global.Set;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var MakeTypeError;
var MapIterator = utils.ImportNow("MapIterator");
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");
var SetIterator = utils.ImportNow("SetIterator");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

function SetIteratorConstructor(set, kind) {
  %SetIteratorInitialize(this, set, kind);
}


function SetIteratorNextJS() {
  if (!IS_SET_ITERATOR(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Set Iterator.prototype.next', this);
  }

  var value_array = [UNDEFINED, UNDEFINED];
  var result = %_CreateIterResultObject(value_array, false);
  switch (%SetIteratorNext(this, value_array)) {
    case 0:
      result.value = UNDEFINED;
      result.done = true;
      break;
    case ITERATOR_KIND_VALUES:
      result.value = value_array[0];
      break;
    case ITERATOR_KIND_ENTRIES:
      value_array[1] = value_array[0];
      break;
  }

  return result;
}


function SetEntries() {
  if (!IS_SET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Set.prototype.entries', this);
  }
  return new SetIterator(this, ITERATOR_KIND_ENTRIES);
}


function SetValues() {
  if (!IS_SET(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Set.prototype.values', this);
  }
  return new SetIterator(this, ITERATOR_KIND_VALUES);
}

// -------------------------------------------------------------------

%SetCode(SetIterator, SetIteratorConstructor);
%FunctionSetInstanceClassName(SetIterator, 'Set Iterator');
utils.InstallFunctions(SetIterator.prototype, DONT_ENUM, [
  'next', SetIteratorNextJS
]);

%AddNamedProperty(SetIterator.prototype, toStringTagSymbol,
    "Set Iterator", READ_ONLY | DONT_ENUM);

utils.InstallFunctions(GlobalSet.prototype, DONT_ENUM, [
  'entries', SetEntries,
  'keys', SetValues,
  'values', SetValues
]);

%AddNamedProperty(GlobalSet.prototype, iteratorSymbol, SetValues, DONT_ENUM);

// -------------------------------------------------------------------

function MapIteratorConstructor(map, kind) {
  %MapIteratorInitialize(this, map, kind);
}


function MapIteratorNextJS() {
  if (!IS_MAP_ITERATOR(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map Iterator.prototype.next', this);
  }

  var value_array = [UNDEFINED, UNDEFINED];
  var result = %_CreateIterResultObject(value_array, false);
  switch (%MapIteratorNext(this, value_array)) {
    case 0:
      result.value = UNDEFINED;
      result.done = true;
      break;
    case ITERATOR_KIND_KEYS:
      result.value = value_array[0];
      break;
    case ITERATOR_KIND_VALUES:
      result.value = value_array[1];
      break;
    // ITERATOR_KIND_ENTRIES does not need any processing.
  }

  return result;
}


function MapEntries() {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.entries', this);
  }
  return new MapIterator(this, ITERATOR_KIND_ENTRIES);
}


function MapKeys() {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.keys', this);
  }
  return new MapIterator(this, ITERATOR_KIND_KEYS);
}


function MapValues() {
  if (!IS_MAP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map.prototype.values', this);
  }
  return new MapIterator(this, ITERATOR_KIND_VALUES);
}

// -------------------------------------------------------------------

%SetCode(MapIterator, MapIteratorConstructor);
%FunctionSetInstanceClassName(MapIterator, 'Map Iterator');
utils.InstallFunctions(MapIterator.prototype, DONT_ENUM, [
  'next', MapIteratorNextJS
]);

%AddNamedProperty(MapIterator.prototype, toStringTagSymbol,
    "Map Iterator", READ_ONLY | DONT_ENUM);


utils.InstallFunctions(GlobalMap.prototype, DONT_ENUM, [
  'entries', MapEntries,
  'keys', MapKeys,
  'values', MapValues
]);

%AddNamedProperty(GlobalMap.prototype, iteratorSymbol, MapEntries, DONT_ENUM);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.MapEntries = MapEntries;
  to.MapIteratorNext = MapIteratorNextJS;
  to.SetIteratorNext = SetIteratorNextJS;
  to.SetValues = SetValues;
});

})
