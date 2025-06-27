// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turboshaft-enable-debug-features

Object.prototype.bla = function() {}

const bla = Object.prototype.bla;

function bar(c) {
  %TurbofanStaticAssert(c.bla === bla);
}

function foo() {
  var c = new Object();
  bar(c);
}
%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
bar({});
bar({a:1});
bar({aa:1});
bar({aaa:1});
bar({aaaa:1});
bar({aaaaa:1});
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
