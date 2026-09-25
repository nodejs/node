// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(o, y) {
  return String(o);
}

%PrepareFunctionForOptimization(f);
%OptimizeMaglevOnNextCall(f);
assertEquals("1", f(1));
assertEquals("undefined", f(undefined));
assertEquals("null", f(null));
assertEquals("bla", f("bla"));
assertEquals("[object Object]", f({}));
assertEquals("Symbol(bla)", f(Symbol('bla')))
