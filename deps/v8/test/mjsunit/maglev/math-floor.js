// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev
function f(o) {
  return Math.floor(o.a);
}

%PrepareFunctionForOptimization(f);
f({a: {valueOf(){ return 7.5; }}});
f({a: {valueOf(){ return 7.5; }}});
f({a: {valueOf(){ return 7.5; }}});
f({a: {valueOf(){ return 7.5; }}});
%OptimizeMaglevOnNextCall(f);
assertEquals(NaN, f({a:NaN}));
assertEquals(Infinity, f({a:Infinity}));
assertEquals(-Infinity, f({a:-Infinity}));
assertEquals(-1, 1/f({a:-0.5}));
assertEquals(Infinity, 1/f({a:0.5}));
assertEquals(7, f({a: {valueOf(){
  %DeoptimizeFunction(f);
  return 7.5; }}}));
