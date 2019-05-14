// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var global = "10.1";
function f() { }
function g(a) { this.d = a; }
function h() {
  var x = new f();
  global.dummy = this;
  var y = new g(x);
}
h();
h();
%OptimizeFunctionOnNextCall(h);
h();
