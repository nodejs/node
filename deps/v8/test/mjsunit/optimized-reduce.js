// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbo-inline-array-builtins
// Flags: --opt --no-always-opt

// Make sure we gracefully handle the case of an empty array in
// optimized code.
(function() {
  var nothingThere = function(only_holes) {
    var a = [1,2,,3];  // holey smi array.
    if (only_holes) {
      a = [,,,];  // also a holey smi array.
    }
    return a.reduce((r,v,i,o)=>r+v);
  }
  nothingThere();
  nothingThere();
  %OptimizeFunctionOnNextCall(nothingThere);
  assertThrows(() => nothingThere(true));
})();

// An error generated inside the callback includes reduce in it's
// stack trace.
(function() {
  var re = /Array\.reduce/;
  var alwaysThrows = function() {
    var b = [,,,];
    var result = 0;
    var callback = function(r,v,i,o) {
      return r + v;
    };
    b.reduce(callback);
  }
  try {
    alwaysThrows();
  } catch (e) {
    assertTrue(re.exec(e.stack) !== null);
  }
  try { alwaysThrows(); } catch (e) {}
  try { alwaysThrows(); } catch (e) {}
  %OptimizeFunctionOnNextCall(alwaysThrows);
  try {
    alwaysThrows();
  } catch (e) {
    assertTrue(re.exec(e.stack) !== null);
  }
})();
