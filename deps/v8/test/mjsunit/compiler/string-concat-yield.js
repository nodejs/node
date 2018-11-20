// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* foo() {
  var f = `foo${ yield 'yielded' }bar`;
  return f;
}

%OptimizeFunctionOnNextCall(foo);
var gen = foo();
assertEquals('yielded', gen.next('unused').value);
assertEquals('foobazbar', gen.next('baz').value);
