// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

  function make_includes() {
    return new Function('result = array.includes(target)');
  }

  createSuite('SmiIncludes', 1000, make_includes(), SmiIncludesSetup);
  createSuite('SparseSmiIncludes', 1000, make_includes(), SparseSmiIncludesSetup);
  createSuite('DoubleIncludes', 1000, make_includes(), SmiIncludesSetup);
  createSuite('SparseDoubleIncludes', 1000, make_includes(), SparseSmiIncludesSetup);
  createSuite('ObjectIncludes', 1000, make_includes(), SmiIncludesSetup);
  createSuite('SparseObjectIncludes', 1000, make_includes(), SparseSmiIncludesSetup);
  createSuite('StringIncludes', 1000, make_includes(), StringIncludesSetup);
  createSuite('SparseStringIncludes', 1000, make_includes(), SparseStringIncludesSetup);

  function SmiIncludesSetup() {
    array = new Array();
    for (let i = 0; i < array_size; ++i) array[i] = i;
    target = array[array_size-1];
  }

  function SparseSmiIncludesSetup() {
    SmiIncludesSetup();
    array.length = array.length * 2;
    target = array[array_size-1];
  }

  function StringIncludesSetup() {
    array = new Array();
    for (let i = 0; i < array_size; ++i) array[i] = `Item no. ${i}`;
    target = array[array_size-1];
  }

  function SparseStringIncludesSetup() {
    StringIncludesSetup();
    array.length = array.length * 2;
    target = array[array_size-1];
  }

  function DoubleIncludesSetup() {
    array = new Array();
    for (let i = 0; i < array_size; ++i) array[i] = i;
    target = array[array_size-1];
  }

  function SparseDoubleIncludesSetup() {
    DoubleIncludesSetup();
    array.length = array.length * 2;
    target = array[array_size-1];
  }

  function ObjectIncludesSetup() {
    array = new Array();
    for (let i = 0; i < array_size; ++i) array[i] = {i};
    target = array[array_size-1];
  }

  function SparseObjectIncludesSetup() {
    ObjectIncludesSetup();
    array.length = array.length * 2;
    target = array[array_size-1];
  }

  })();
