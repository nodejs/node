// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function foo(bar) {
  return bar.func();
}
class Bar {
  func() {}
}
Bar.prototype.__proto__ = new Proxy(Bar.prototype.__proto__, {
  get() {;
    return "42";
  }
});
%PrepareFunctionForOptimization(foo);
foo(new Bar());
foo(new Bar());
%OptimizeMaglevOnNextCall(foo);
foo(new Bar());

function foo_primitive(s) {
  return s.substring();
}
String.prototype.__proto__ = new Proxy(String.prototype.__proto__, {
  get() {;
    return "42";
  }
});
%PrepareFunctionForOptimization(foo_primitive);
foo_primitive("");
foo_primitive("");
%OptimizeMaglevOnNextCall(foo_primitive);
foo_primitive("");
