// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --enable-mega-dom-ic --allow-natives-syntax

// This tests checks that load property access using megadom IC returns
// correct results on API.

function load(obj) {
  return obj.nodeType;
}
%PrepareFunctionForOptimization(load);

var a = new d8.dom.Div();
var b = new d8.dom.Div();
b.b = 1;

var c = new d8.dom.Div();
c.c = 1;

var d = new d8.dom.Div();
d.d = 1;

var e = new d8.dom.Div();
e.e = 1;

var f = new d8.dom.Div();
f.f = 1;

const objs = [
  a, b, c, d, e, f
];

function test() {
  let result = 0;
  for (let i = 0; i < objs.length; i++) {
    result += load(objs[i]);
  }

  return result;
}
%PrepareFunctionForOptimization(test);
let result = test();
assertEquals(6, result);

%OptimizeFunctionOnNextCall(test);
result = test();
assertEquals(6, result);
