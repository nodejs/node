// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function test1(s) {
  return s.slice(1, 3);
}
%PrepareFunctionForOptimization(test1);

test1('foo');

%OptimizeFunctionOnNextCall(test1);

assertEquals('oo', test1('foo'));
assertEquals('o', test1('fo'));
assertEquals('', test1('f'));
assertEquals('', test1(''));

function test2(s) {
  return s.slice(-2, 3);
}
%PrepareFunctionForOptimization(test2);

test2('foo');

%OptimizeFunctionOnNextCall(test2);

assertEquals('oo', test2('foo'));
assertEquals('fo', test2('fo'));
assertEquals('f', test2('f'));
assertEquals('', test2(''));

function test3(s) {
  return s.slice(-3, -1);
}
%PrepareFunctionForOptimization(test3);

test3('foo');

%OptimizeFunctionOnNextCall(test3);

assertEquals('fo', test3('foo'));
assertEquals('f', test3('fo'));
assertEquals('', test3('f'));
assertEquals('', test3(''));

function test4(s) {
  return s.slice(-2147483648, -1);
}
%PrepareFunctionForOptimization(test4);

test4('foo');

%OptimizeFunctionOnNextCall(test4);

assertEquals('fo', test4('foo'));
assertEquals('f', test4('fo'));
assertEquals('', test4('f'));
assertEquals('', test4(''));

function test5(s) {
  return s.slice(0, -2147483648);
}
%PrepareFunctionForOptimization(test5);

test5('foo');

%OptimizeFunctionOnNextCall(test5);

assertEquals('', test5('foo'));
assertEquals('', test5('fo'));
assertEquals('', test5('f'));
assertEquals('', test5(''));

function test6(s) {
  return s.slice(3, 2); // always empty
}
%PrepareFunctionForOptimization(test6);

test6('foo');

%OptimizeFunctionOnNextCall(test6);

assertEquals('', test6('foo'));
assertEquals('', test6('fo'));
assertEquals('', test6('f'));
assertEquals('', test6(''));

function test7(s) {
  return s.slice(-2, -3); // always empty
}
%PrepareFunctionForOptimization(test7);

test7('foo');

%OptimizeFunctionOnNextCall(test7);

assertEquals('', test7('foo'));
assertEquals('', test7('fo'));
assertEquals('', test7('f'));
assertEquals('', test7(''));

function test8(s) {
  let s1 = s.slice(1, 3);
  let s2 = s.slice(1, 3);
  %TurbofanStaticAssert(s1 == s2);
  return s1;
}
%PrepareFunctionForOptimization(test8);

test8('foo');

%OptimizeFunctionOnNextCall(test8);

assertEquals('oo', test8('foo'));

let valueOfCalled = 0;
var evil = {
  valueOf: () => {++valueOfCalled; return 1;}
};

function test9(s) {
  let s1 = s.slice(evil, 2);
  let s2 = s.slice(evil, 2); // Cannot be elided
  return [s1, s2];
}
%PrepareFunctionForOptimization(test9);

test9('foo');
assertEquals(2, valueOfCalled);

%OptimizeFunctionOnNextCall(test9);

assertEquals(['o', 'o'], test9('foo'));
assertEquals(4, valueOfCalled);

function test10(s, start, end) {
  return s.slice(start, end);
}
%PrepareFunctionForOptimization(test10)
assertEquals("oo", test10("foo", 1, 3));

%OptimizeFunctionOnNextCall(test10);
assertEquals("oo", test10("foo", 1, 3));
assertOptimized(test10);

// Deopts when start or end aren't numbers.
assertEquals("", test10("foo", 1, "not a number"));
assertUnoptimized(test10);

// Reopting
%OptimizeFunctionOnNextCall(test10);
assertEquals("oo", test10("foo", 1, 3));
assertOptimized(test10);

// This time doesn't deopt anymore (==> no deopt loop)
assertEquals("", test10("foo", 1, "not a number"));
assertOptimized(test10);
