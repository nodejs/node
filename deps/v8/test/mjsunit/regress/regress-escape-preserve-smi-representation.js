// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function deepEquals(a, b) {
  if (a === b) { if (a === 0) return (1 / a) === (1 / b); return true; }
  if (typeof a != typeof b) return false;
  if (typeof a == "number") return isNaN(a) && isNaN(b);
  if (typeof a !== "object" && typeof a !== "function") return false;
  if (objectClass === "RegExp") { return (a.toString() === b.toString()); }
  if (objectClass === "Function") return false;
  if (objectClass === "Array") {
    var elementsCount = 0;
    if (a.length != b.length) { return false; }
    for (var i = 0; i < a.length; i++) {
      if (!deepEquals(a[i], b[i])) return false;
    }
    return true;
  }
}


function __f_1(){
  var __v_0 = [];
  for(var i=0; i<2; i++){
    __v_0.push([])
    deepEquals(2, __v_0.length);
  }
}
__f_1();
%OptimizeFunctionOnNextCall(__f_1);
__f_1();
