// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function iterateArray() {
  var array = new Array();
  var it = array.entries();
  it.next();
}

function iterateTypedArray() {
  var array = new Uint8Array();
  var it = array.entries();
  it.next();
}

function testArray() {
  iterateArray();
  try {
  } catch (e) {
  }
}
testArray();
testArray();
%OptimizeFunctionOnNextCall(testArray);
testArray();

function testTypedArray() {
  iterateTypedArray();
  try {
  } catch (e) {
  }
}
testTypedArray();
testTypedArray();
%OptimizeFunctionOnNextCall(testTypedArray);
testTypedArray();
