// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";


// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Set = global.Set;
// var $Map = global.Map;


function SetIteratorConstructor(set, kind) {
  %SetIteratorInitialize(this, set, kind);
}


function SetIteratorNextJS() {
  if (!IS_SET_ITERATOR(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set Iterator.prototype.next', this]);
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
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.entries', this]);
  }
  return new SetIterator(this, ITERATOR_KIND_ENTRIES);
}


function SetValues() {
  if (!IS_SET(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Set.prototype.values', this]);
  }
  return new SetIterator(this, ITERATOR_KIND_VALUES);
}


function SetUpSetIterator() {
  %CheckIsBootstrapping();

  %SetCode(SetIterator, SetIteratorConstructor);
  %FunctionSetPrototype(SetIterator, new $Object());
  %FunctionSetInstanceClassName(SetIterator, 'Set Iterator');
  InstallFunctions(SetIterator.prototype, DONT_ENUM, $Array(
    'next', SetIteratorNextJS
  ));

  %FunctionSetName(SetIteratorSymbolIterator, '[Symbol.iterator]');
  %AddNamedProperty(SetIterator.prototype, symbolIterator,
      SetIteratorSymbolIterator, DONT_ENUM);
  %AddNamedProperty(SetIterator.prototype, symbolToStringTag,
      "Set Iterator", READ_ONLY | DONT_ENUM);
}

SetUpSetIterator();


function ExtendSetPrototype() {
  %CheckIsBootstrapping();

  InstallFunctions($Set.prototype, DONT_ENUM, $Array(
    'entries', SetEntries,
    'keys', SetValues,
    'values', SetValues
  ));

  %AddNamedProperty($Set.prototype, symbolIterator, SetValues, DONT_ENUM);
}

ExtendSetPrototype();



function MapIteratorConstructor(map, kind) {
  %MapIteratorInitialize(this, map, kind);
}


function MapIteratorSymbolIterator() {
  return this;
}


function MapIteratorNextJS() {
  if (!IS_MAP_ITERATOR(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map Iterator.prototype.next', this]);
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
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.entries', this]);
  }
  return new MapIterator(this, ITERATOR_KIND_ENTRIES);
}


function MapKeys() {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.keys', this]);
  }
  return new MapIterator(this, ITERATOR_KIND_KEYS);
}


function MapValues() {
  if (!IS_MAP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['Map.prototype.values', this]);
  }
  return new MapIterator(this, ITERATOR_KIND_VALUES);
}


function SetUpMapIterator() {
  %CheckIsBootstrapping();

  %SetCode(MapIterator, MapIteratorConstructor);
  %FunctionSetPrototype(MapIterator, new $Object());
  %FunctionSetInstanceClassName(MapIterator, 'Map Iterator');
  InstallFunctions(MapIterator.prototype, DONT_ENUM, $Array(
    'next', MapIteratorNextJS
  ));

  %FunctionSetName(MapIteratorSymbolIterator, '[Symbol.iterator]');
  %AddNamedProperty(MapIterator.prototype, symbolIterator,
      MapIteratorSymbolIterator, DONT_ENUM);
  %AddNamedProperty(MapIterator.prototype, symbolToStringTag,
      "Map Iterator", READ_ONLY | DONT_ENUM);
}

SetUpMapIterator();


function ExtendMapPrototype() {
  %CheckIsBootstrapping();

  InstallFunctions($Map.prototype, DONT_ENUM, $Array(
    'entries', MapEntries,
    'keys', MapKeys,
    'values', MapValues
  ));

  %AddNamedProperty($Map.prototype, symbolIterator, MapEntries, DONT_ENUM);
}

ExtendMapPrototype();
