// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo(...args) {
  return {'this': this, 'arguments': args};
}

function validCalls(a, ...args) {
  let obj = {};

  // No receiver or receiver is null or undefined.
  assertEquals({'this': this, 'arguments': []}, foo.apply());
  assertEquals({'this': this, 'arguments': []}, foo.apply(null));
  assertEquals({'this': this, 'arguments': []}, foo.apply(undefined));
  // Receiver is object.
  assertEquals({'this': obj, 'arguments': []}, foo.apply(obj));
  // Valid arguments array.
  assertEquals({'this': this, 'arguments': [3, 4]}, foo.apply(null, args));
  assertEquals({'this': obj, 'arguments': [3, 4]}, foo.apply(obj, args));
  assertEquals(
      {'this': this, 'arguments': [2, 3, 4]}, foo.apply(null, [a, ...args]));
  assertEquals(
      {'this': obj, 'arguments': [2, 3, 4]}, foo.apply(obj, [a, ...args]));
  // Extra arguments are ignored.
  assertEquals({'this': this, 'arguments': [3, 4]}, foo.apply(null, args, a));
  assertEquals({'this': obj, 'arguments': [3, 4]}, foo.apply(obj, args, a));
}

%PrepareFunctionForOptimization(validCalls);
validCalls(2, 3, 4);
validCalls(2, 3, 4);
%OptimizeMaglevOnNextCall(validCalls);
validCalls(2, 3, 4);

function invalidArgsArrayExplicitReceiver(a) {
  return foo.apply(null, a);
}

%PrepareFunctionForOptimization(invalidArgsArrayExplicitReceiver);
assertThrows(
    () => invalidArgsArrayExplicitReceiver(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertThrows(
    () => invalidArgsArrayExplicitReceiver(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertEquals(
    {'this': this, 'arguments': []},
    invalidArgsArrayExplicitReceiver(null, null, null));
%OptimizeMaglevOnNextCall(invalidArgsArrayExplicitReceiver);
assertThrows(
    () => invalidArgsArrayExplicitReceiver(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertEquals(
    {'this': this, 'arguments': []},
    invalidArgsArrayExplicitReceiver(null, null, null));

function invalidArgsArrayImplicitReceiver(...args) {
  return foo.apply(...args);
}

%PrepareFunctionForOptimization(invalidArgsArrayImplicitReceiver);
assertThrows(
    () => invalidArgsArrayImplicitReceiver(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertThrows(
    () => invalidArgsArrayImplicitReceiver(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertEquals(
    {'this': this, 'arguments': []},
    invalidArgsArrayImplicitReceiver(null, null, null));
%OptimizeMaglevOnNextCall(invalidArgsArrayImplicitReceiver);
assertThrows(
    () => invalidArgsArrayImplicitReceiver(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertEquals(
    {'this': this, 'arguments': []},
    invalidArgsArrayImplicitReceiver(null, null, null));

function invalidArgsArrayWithExtraSpread(a, ...args) {
  return foo.apply(null, a, ...args);
}

%PrepareFunctionForOptimization(invalidArgsArrayWithExtraSpread);
assertThrows(
    () => invalidArgsArrayWithExtraSpread(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertThrows(
    () => invalidArgsArrayWithExtraSpread(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertEquals(
    {'this': this, 'arguments': []},
    invalidArgsArrayWithExtraSpread(null, null, null));
%OptimizeMaglevOnNextCall(invalidArgsArrayWithExtraSpread);
assertThrows(
    () => invalidArgsArrayWithExtraSpread(2, 3, 4), TypeError,
    'CreateListFromArrayLike called on non-object');
assertEquals(
    {'this': this, 'arguments': []},
    invalidArgsArrayWithExtraSpread(null, null, null));

function nullArgsArray(a, ...args) {
  assertEquals({'this': this, 'arguments': []}, foo.apply(null, null));
}

%PrepareFunctionForOptimization(nullArgsArray);
nullArgsArray(2, 3, 4);
nullArgsArray(2, 3, 4);
nullArgsArray(null, null, null);
%OptimizeMaglevOnNextCall(nullArgsArray);
nullArgsArray(2, 3, 4);
nullArgsArray(null, null, null);

function nullArgsArrayWithExtraSpread(a, ...args) {
  assertEquals({'this': this, 'arguments': []}, foo.apply(null, null, ...args));
}

%PrepareFunctionForOptimization(nullArgsArrayWithExtraSpread);
nullArgsArrayWithExtraSpread(2, 3, 4);
nullArgsArrayWithExtraSpread(2, 3, 4);
nullArgsArrayWithExtraSpread(null, null, null);
%OptimizeMaglevOnNextCall(nullArgsArrayWithExtraSpread);
nullArgsArrayWithExtraSpread(2, 3, 4);
nullArgsArrayWithExtraSpread(null, null, null);
