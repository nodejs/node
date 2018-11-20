// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a) {
  a = "" + Math.abs(a);
  return a.charCodeAt(0);
}

// Add '1' to the number to string table (as SeqString).
String.fromCharCode(49);

// Turn the SeqString into a ThinString via forced internalization.
const o = {};
o[(1).toString()] = 1;

assertEquals(49, foo(1));
assertEquals(49, foo(1));
%OptimizeFunctionOnNextCall(foo);
assertEquals(49, foo(1));
