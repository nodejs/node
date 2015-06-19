// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $mapEntries;
var $mapIteratorNext;
var $setIteratorNext;
var $setValues;

(function(global, shared, exports) {

"use strict";

%CheckIsBootstrapping();

var GlobalMap = global.Map;
var GlobalObject = global.Object;
var GlobalSet = global.Set;

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
  var entry = {value: value_array, done: false};
  switch (%SetIteratorNext(this, value_array)) {
    case 0:
      entry.value = UNDEFINED;
      entry.done = true;
      break;
    case ITERATOR_KIND_VALUES:
      entry.value = value_array[0];
      break;
    case ITERATOR_KIND_ENTRIES:
      value_array[1] = value_array[0];
      break;
  }

  return entry;
}


function SetIteratorSymbolIterator() {
  return this;
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
%FunctionSetPrototype(SetIterator, new GlobalObject());
%FunctionSetInstanceClassName(SetIterator, 'Set Iterator');
$installFunctions(SetIterator.prototype, DONT_ENUM, [
  'next', SetIteratorNextJS
]);

$setFunctionName(SetIteratorSymbolIterator, symbolIterator);
%AddNamedProperty(SetIterator.prototype, symbolIterator,
    SetIteratorSymbolIterator, DONT_ENUM);
%AddNamedProperty(SetIterator.prototype, symbolToStringTag,
    "Set Iterator", READ_ONLY | DONT_ENUM);

$installFunctions(GlobalSet.prototype, DONT_ENUM, [
  'entries', SetEntries,
  'keys', SetValues,
  'values', SetValues
]);

%AddNamedProperty(GlobalSet.prototype, symbolIterator, SetValues, DONT_ENUM);

$setIteratorNext = SetIteratorNextJS;
$setValues = SetValues;

// -------------------------------------------------------------------

function MapIteratorConstructor(map, kind) {
  %MapIteratorInitialize(this, map, kind);
}


function MapIteratorSymbolIterator() {
  return this;
}


function MapIteratorNextJS() {
  if (!IS_MAP_ITERATOR(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Map Iterator.prototype.next', this);
  }

  var value_array = [UNDEFINED, UNDEFINED];
  var entry = {value: value_array, done: false};
  switch (%MapIteratorNext(this, value_array)) {
    case 0:
      entry.value = UNDEFINED;
      entry.done = true;
      break;
    case ITERATOR_KIND_KEYS:
      entry.value = value_array[0];
      break;
    case ITERATOR_KIND_VALUES:
      entry.value = value_array[1];
      break;
    // ITERATOR_KIND_ENTRIES does not need any processing.
  }

  return entry;
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
%FunctionSetPrototype(MapIterator, new GlobalObject());
%FunctionSetInstanceClassName(MapIterator, 'Map Iterator');
$installFunctions(MapIterator.prototype, DONT_ENUM, [
  'next', MapIteratorNextJS
]);

$setFunctionName(MapIteratorSymbolIterator, symbolIterator);
%AddNamedProperty(MapIterator.prototype, symbolIterator,
    MapIteratorSymbolIterator, DONT_ENUM);
%AddNamedProperty(MapIterator.prototype, symbolToStringTag,
    "Map Iterator", READ_ONLY | DONT_ENUM);


$installFunctions(GlobalMap.prototype, DONT_ENUM, [
  'entries', MapEntries,
  'keys', MapKeys,
  'values', MapValues
]);

%AddNamedProperty(GlobalMap.prototype, symbolIterator, MapEntries, DONT_ENUM);

$mapEntries = MapEntries;
$mapIteratorNext = MapIteratorNextJS;

})
