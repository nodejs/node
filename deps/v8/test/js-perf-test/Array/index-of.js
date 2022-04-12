// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

  function make_indexOf() {
    return new Function('result = array.indexOf(target)');
  }

  createSuite('SmiIndexOf', 1000, make_indexOf(), SmiIndexOfSetup);
  createSuite('SparseSmiIndexOf', 1000, make_indexOf(), SparseSmiIndexOfSetup);
  createSuite('DoubleIndexOf', 1000, make_indexOf(), SmiIndexOfSetup);
  createSuite('SparseDoubleIndexOf', 1000, make_indexOf(), SparseSmiIndexOfSetup);
  createSuite('ObjectIndexOf', 1000, make_indexOf(), SmiIndexOfSetup);
  createSuite('SparseObjectIndexOf', 1000, make_indexOf(), SparseSmiIndexOfSetup);
  createSuite('StringIndexOf', 1000, make_indexOf(), StringIndexOfSetup);
  createSuite('SparseStringIndexOf', 1000, make_indexOf(), SparseStringIndexOfSetup);

  function SmiIndexOfSetup() {
    array = new Array();
    for (let i = 0; i < array_size; ++i) array[i] = i;
    target = array[array_size-1];
  }

  function SparseSmiIndexOfSetup() {
    SmiIndexOfSetup();
    array.length = array.length * 2;
    target = array[array_size-1];
  }

  function StringIndexOfSetup() {
    array = new Array();
    for (let i = 0; i < array_size; ++i) array[i] = `Item no. ${i}`;
    target = array[array_size-1];
  }

  function SparseStringIndexOfSetup() {
    StringIndexOfSetup();
    array.length = array.length * 2;
    target = array[array_size-1];
  }

  function DoubleIndexOfSetup() {
    array = new Array();
    for (let i = 0; i < array_size; ++i) array[i] = i;
    target = array[array_size-1];
  }

  function SparseDoubleIndexOfSetup() {
    DoubleIndexOfSetup();
    array.length = array.length * 2;
    target = array[array_size-1];
  }

  function ObjectIndexOfSetup() {
    array = new Array();
    for (let i = 0; i < array_size; ++i) array[i] = {i};
    target = array[array_size-1];
  }

  function SparseObjectIndexOfSetup() {
    ObjectIndexOfSetup();
    array.length = array.length * 2;
    target = array[array_size-1];
  }

  })();
