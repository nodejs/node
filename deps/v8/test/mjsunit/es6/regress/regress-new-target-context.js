// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test access of the new.target value in functions that also allocate local
// function contexts of varying sizes, making sure the value is not clobbered.

function makeFun(n) {
  var source = "(function f" + n + "() { ";
  for (var i = 0; i < n; ++i) source += "var v" + i + "; ";
  source += "(function() { 0 ";
  for (var i = 0; i < n; ++i) source += "+ v" + i + " ";
  source += "})(); return { value: new.target }; })";
  return eval(source);
}

// Exercise fast case.
var a = makeFun(4);
assertEquals(a, new a().value);
assertEquals(undefined, a().value);

// Exercise slow case.
var b = makeFun(128);
assertEquals(b, new b().value);
assertEquals(undefined, b().value);
