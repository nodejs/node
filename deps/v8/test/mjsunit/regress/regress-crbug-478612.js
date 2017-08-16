// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This is used to force binary operations below to have tagged representation.
var z = {valueOf: function() { return 3; }};


function f() {
  var y = -2;
  return (1 & z) - y++;
}

assertEquals(3, f());
assertEquals(3, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(3, f());


function g() {
  var y = 2;
  return (1 & z) | y++;
}

assertEquals(3, g());
assertEquals(3, g());
%OptimizeFunctionOnNextCall(g);
assertEquals(3, g());


function h() {
  var y = 3;
  return (3 & z) & y++;
}

assertEquals(3, h());
assertEquals(3, h());
%OptimizeFunctionOnNextCall(h);
assertEquals(3, h());


function i() {
  var y = 2;
  return (1 & z) ^ y++;
}

assertEquals(3, i());
assertEquals(3, i());
%OptimizeFunctionOnNextCall(i);
assertEquals(3, i());
