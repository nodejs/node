// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --no-always-turbofan

function foo(set, value) {
  return set.has(value);
}
%PrepareFunctionForOptimization(foo);

const s1 = new Set();
s1.add('foo');
s1.add('bar');
s1.add('baz');

assertTrue(foo(s1, 'foo'));
assertFalse(foo(s1, {}));

%OptimizeFunctionOnNextCall(foo);

assertTrue(foo(s1, 'foo'));
assertFalse(foo(s1, {}));

{
  const string1 = 'fo';
  const string2 = 'o';
  assertTrue(foo(s1, string1 + string2));
}

const s2 = new Set();
const o1 = {};
const o2 = {};
s2.add(o1);
s2.add(o2);

assertTrue(foo(s2, o1));
assertTrue(foo(s2, o2));
assertFalse(foo(s2, {}));
assertFalse(foo(s2, 'foo'));
assertFalse(foo(s2, 1));
