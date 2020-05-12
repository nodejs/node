// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --opt

function make_closure() { return () => { return 42; } }
%PrepareFunctionForOptimization(make_closure);
%PrepareFunctionForOptimization(make_closure());

function inline_polymorphic(f) {
  let answer = f();
  %TurbofanStaticAssert(answer == 42);
}

%PrepareFunctionForOptimization(inline_polymorphic);
inline_polymorphic(make_closure());
inline_polymorphic(make_closure());
%OptimizeFunctionOnNextCall(inline_polymorphic);
inline_polymorphic(make_closure());

try {
  inline_polymorphic(3);
} catch(e) {}
