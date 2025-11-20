// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var foo = 'foo';
var bar = 'bar';

var a;
var b;
var c;
var d;

function baz() { return 'baz'; }

function test(arg) {
  // All add operations are constant folded by native context
  // specialization / inlining.
  a = '"' + foo + '-' + bar + '"';
  b = '"' + foo + '-' + baz() + '"';

  // Reduce down to a JSAdd of folded constant + arg.
  c = foo + bar + arg;

  // Reduces to two JSAdds with other arguments constant folded.
  d = '"' + foo + arg + bar + '"';
}

%PrepareFunctionForOptimization(test);
test('boo');
%OptimizeFunctionOnNextCall(test);
test('baa');

assertEquals('"foo-bar"', a);
assertEquals('"foo-baz"', b);
assertEquals('foobarbaa', c);
assertEquals('"foobaabar"', d);
