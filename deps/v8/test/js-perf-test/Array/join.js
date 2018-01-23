// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

var array;
var result;
var array_size = 1000;

function make_join() {
  return new Function('result = array.join();');
}

benchy('SmiJoin', make_join(), SmiJoinSetup);
benchy('StringJoin', make_join(), StringJoinSetup);
benchy('SparseSmiJoin', make_join(), SparseSmiJoinSetup);
benchy('SparseStringJoin', make_join(), SparseStringJoinSetup);

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

})();
