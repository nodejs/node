// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(name, test, testSetup) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, ()=>{})
      ]);
}

benchy('SmiJoin', SmiJoin, SmiJoinSetup);
benchy('StringJoin', StringJoin, StringJoinSetup);
benchy('SparseSmiJoin', SparseSmiJoin, SparseSmiJoinSetup);
benchy('SparseStringJoin', SparseStringJoin, SparseStringJoinSetup);

var array;
var result;
var array_size = 1000;


// Although these functions have the same code, they are separated for
// clean IC feedback.
function SmiJoin() {
  result = array.join();
}
function StringJoin() {
  result = array.join();
}
function SparseSmiJoin() {
  result = array.join();
}
function SparseStringJoin() {
  result = array.join();
}

function SmiJoinSetup() {
  array = new Array();
  for (var i = 0; i < array_size; ++i) array[i] = i;
}
function StringJoinSetup() {
  array = new Array();
  for (var i = 0; i < array_size; ++i) array[i] = `Item no. ${i}`;
}
function SparseSmiJoinSetup() {
  SmiJoinSetup();
  array.length = array.length * 2;
}
function SparseStringJoinSetup() {
  StringJoinSetup();
  array.length = array.length * 2;
}
