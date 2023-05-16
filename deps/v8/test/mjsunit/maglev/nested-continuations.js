// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function Foo() {}
Object.defineProperty(Foo, Symbol.hasInstance, { value: Math.round });

let foo = new Foo();

function bar(f) {
  // `f instanceof Foo` runs `%ToBoolean(Foo[Symbol.hasInstance](f))`, where
  // `Foo[Symbol.hasInstance]` is `Math.round`.
  //
  // So with sufficient builtin inlining, this will call
  // `%ToBoolean(round(%ToNumber(f)))`, which will call `f.valueOf`. If this
  // deopts (which in this test it will), we need to make sure to both round it,
  // and then convert that rounded value to a boolean.
  return f instanceof Foo;
}

foo.valueOf = () => {
  %DeoptimizeFunction(bar);
  // Return a value which, when rounded, has ToBoolean false, and when not
  // rounded, has ToBoolean true.
  return 0.2;
}

%PrepareFunctionForOptimization(bar);
assertFalse(bar(foo));
assertFalse(bar(foo));

%OptimizeMaglevOnNextCall(bar);
assertFalse(bar(foo));
