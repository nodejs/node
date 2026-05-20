// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

let a = 42;
function* f() {
  try {
    let a = 10;
    yield 2;
    () => a;
    return "fail"
  } catch (e) {
    return a;
  }
}

%PrepareFunctionForOptimization(f);
let g = f();
assertEquals({value:2, done:false}, g.next());
assertEquals({value:42, done:true}, g.throw("hello"));

%OptimizeFunctionOnNextCall(f);
g = f();
assertEquals({value:2, done:false}, g.next());
assertEquals({value:42, done:true}, g.throw("hello"));
