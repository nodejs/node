// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const one_byte = 'AAAAAAAA';
const two_byte = 'ሴሴሴሴሴሴሴሴ';

const one_byte_cons = %ConstructConsString(one_byte, one_byte);
const two_byte_cons = %ConstructConsString(two_byte, two_byte);

function foo(str) {
  // Create ConsString without constant map.
  const cons = str + 'XXXXXXXX';
  return cons + cons;
}

function test() {
  assertTrue(%HaveSameMap(one_byte_cons, foo(one_byte)));
  assertTrue(%HaveSameMap(two_byte_cons, foo(two_byte)));
}

%PrepareFunctionForOptimization(foo);
test();
%OptimizeFunctionOnNextCall(foo);
test();
