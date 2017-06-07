// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(name, test, testSetup) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, ()=>{})
      ]);
}

benchy('NaiveMapReplacement', NaiveMap, NaiveMapSetup);
benchy('DoubleMap', DoubleMap, DoubleMapSetup);
benchy('SmiMap', SmiMap, SmiMapSetup);
benchy('FastMap', FastMap, FastMapSetup);
benchy('ObjectMap', GenericMap, ObjectMapSetup);

var array;
var func;
var this_arg;
var result;
var array_size = 100;

// Although these functions have the same code, they are separated for
// clean IC feedback.
function DoubleMap() {
  result = array.map(func, this_arg);
}
function SmiMap() {
  result = array.map(func, this_arg);
}
function FastMap() {
  result = array.map(func, this_arg);
}

function NaiveMap() {
  let index = -1
  const length = array == null ? 0 : array.length
  const result = new Array(length)

  while (++index < length) {
    result[index] = func(array[index], index, array)
  }
  return result
}


function GenericMap() {
  result = Array.prototype.map.call(array, func, this_arg);
}

function NaiveMapSetup() {
  // Prime NaiveMap with polymorphic cases.
  array = [1, 2, 3];
  func = (v, i, a) => v;
  NaiveMap();
  NaiveMap();
  array = [3.4]; NaiveMap();
  array = new Array(10); array[0] = 'hello'; NaiveMap();
  SmiMapSetup();
  delete array[1];
}

function SmiMapSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = i;
  func = (value, index, object) => { return value; };
}

function DoubleMapSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = (i + 0.5);
  func = (value, index, object) => { return value; };
}

function FastMapSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = 'value ' + i;
  func = (value, index, object) => { return value; };
}

function ObjectMapSetup() {
  array = { length: array_size };
  for (var i = 0; i < array_size; i++) {
    array[i] = i;
  }
  func = (value, index, object) => { return value; };
}
