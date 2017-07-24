// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(name, test, testSetup) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, ()=>{})
      ]);
}

benchy('NaiveFilterReplacement', NaiveFilter, NaiveFilterSetup);
benchy('DoubleFilter', DoubleFilter, DoubleFilterSetup);
benchy('SmiFilter', SmiFilter, SmiFilterSetup);
benchy('FastFilter', FastFilter, FastFilterSetup);
benchy('ObjectFilter', GenericFilter, ObjectFilterSetup);

var array;
var func;
var this_arg;
var result;
var array_size = 100;

// Although these functions have the same code, they are separated for
// clean IC feedback.
function DoubleFilter() {
  result = array.filter(func, this_arg);
}
function SmiFilter() {
  result = array.filter(func, this_arg);
}
function FastFilter() {
  result = array.filter(func, this_arg);
}

function GenericFilter() {
  result = Array.prototype.filter.call(array, func, this_arg);
}

// From the lodash implementation.
function NaiveFilter() {
  let index = -1
  let resIndex = 0
  const length = array == null ? 0 : array.length
  const result = []

  while (++index < length) {
    const value = array[index]
    if (func(value, index, array)) {
      result[resIndex++] = value
    }
  }
  return result
}

function NaiveFilterSetup() {
  // Prime NaiveFilter with polymorphic cases.
  array = [1, 2, 3];
  func = ()=>true;
  NaiveFilter();
  NaiveFilter();
  array = [3.4]; NaiveFilter();
  array = new Array(10); array[0] = 'hello'; NaiveFilter();
  SmiFilterSetup();
  delete array[1];
}

function SmiFilterSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = i;
  func = (value, index, object) => { return value % 2 === 0; };
}

function DoubleFilterSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = (i + 0.5);
  func = (value, index, object) => { return Math.floor(value) % 2 === 0; };
}

function FastFilterSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = 'value ' + i;
  func = (value, index, object) => { return index % 2 === 0; };
}

function ObjectFilterSetup() {
  array = { length: array_size };
  for (var i = 0; i < array_size; i++) {
    array[i] = i;
  }
  func = (value, index, object) => { return index % 2 === 0; };
}
