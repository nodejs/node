// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --track-field-types --use-gvn

function A(id) {
  this.id = id;
}

var a1 = new A(1);
var a2 = new A(2);


var g;
function f(o, value) {
  g = o.o;
  o.o = value;
  return o.o;
}

var obj = {o: a1};

f(obj, a1);
f(obj, a1);
%OptimizeFunctionOnNextCall(f);
assertEquals(a2.id, f(obj, a2).id);
