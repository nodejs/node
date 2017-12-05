// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(name, test, testSetup) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, ()=>{})
      ]);
}

benchy('SmiToString', SmiToString, SmiToStringSetup);
benchy('StringToString', StringToString, StringToStringSetup);
benchy('SparseSmiToString', SparseSmiToString, SparseSmiToStringSetup);
benchy('SparseStringToString', SparseStringToString, SparseStringToStringSetup);

var array;
var result;
var array_size = 1000;


// Although these functions have the same code, they are separated for
// clean IC feedback.
function SmiToString() {
  result = array.toString();
}
function StringToString() {
  result = array.toString();
}
function SparseSmiToString() {
  result = array.toString();
}
function SparseStringToString() {
  result = array.toString();
}

function SmiToStringSetup() {
  array = new Array();
  for (var i = 0; i < array_size; ++i) array[i] = i;
}
function StringToStringSetup() {
  array = new Array();
  for (var i = 0; i < array_size; ++i) array[i] = `Item no. ${i}`;
}
function SparseSmiToStringSetup() {
  SmiToStringSetup();
  array.length = array.length * 2;
}
function SparseStringToStringSetup() {
  StringToStringSetup();
  array.length = array.length * 2;
}
