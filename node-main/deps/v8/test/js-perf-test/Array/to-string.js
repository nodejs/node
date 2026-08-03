// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

function make_tostring() {
  return new Function("result = array.toString();");
}

createSuite('SmiToString', 1000, make_tostring(), SmiToStringSetup);
createSuite('StringToString', 1000, make_tostring(), StringToStringSetup);
createSuite('SparseSmiToString', 1000, make_tostring(), SparseSmiToStringSetup);
createSuite(
    'SparseStringToString', 1000, make_tostring(), SparseStringToStringSetup);

var array;
var result;
var array_size = 1000;

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

})();
