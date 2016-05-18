// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-slow-asserts

Array.prototype.__proto__ = null;
var func = Array.prototype.push;
var prototype = Array.prototype;
function CallFunc(a) {
  func.call(a);
}
function CallFuncWithPrototype() {
  CallFunc(prototype);
}
CallFunc([]);
CallFunc([]);
%OptimizeFunctionOnNextCall(CallFuncWithPrototype);
CallFuncWithPrototype();
