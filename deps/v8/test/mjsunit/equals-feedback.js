// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Make sure the tested functions won't hit the eval cache.
let counter = 0;
function testOneWay(a, b, eq, expectedEquals, expectedFeedback) {
  const equalsFunction = eval(
    'function f' + counter +'(a, b) { return a ' + eq + ' b;} f' + counter);
  ++counter;
  %PrepareFunctionForOptimization(equalsFunction);
  assertEquals(expectedEquals, equalsFunction(a, b));
  const feedback = %GetFeedback(equalsFunction);
  if (feedback === undefined) {
    // Feedback -> string conversion not enabled in this build.
    return;
  }
  assertMatches(new RegExp('CompareOp:' + expectedFeedback), feedback[0][1]);
}

function testLoose(a, b, expectedEquals, expectedFeedback) {
  testOneWay(a, b, '==', expectedEquals, expectedFeedback);
  testOneWay(b, a, '==', expectedEquals, expectedFeedback);
}

function testStrict(a, b, expectedEquals, expectedFeedback) {
  testOneWay(a, b, '===', expectedEquals, expectedFeedback);
  testOneWay(b, a, '===', expectedEquals, expectedFeedback);
}

function test(a, b, expectedEquals, expectedFeedback) {
  testLoose(a, b, expectedEquals, expectedFeedback);
  testStrict(a, b, expectedEquals, expectedFeedback);
}

// lhs and rhs can be, independently of each other:
// SMI
// HeapNumber
// - NaN
// - other HeapNumber
// BigInt(64)
// Non-null-or-undefined undetectable (document.all)
// Oddball
// - Boolean
//   - true
//   - false
// - null
// - undefined
// String
// - internalized string
// - other string
// Symbol
// receiver (object)

// We detect the following compare operation types:
// SignedSmall
// Number
// NumberOrBoolean
// NumberOrOddball
// InternalizedString
// String
// Symbol
// BigInt
// BigInt64
// Receiver
// ReceiverOrNullOrUndefined
// Any

// SMI, SMI
test(-14, -14, true, 'SignedSmall');
test(15, 16, false, 'SignedSmall');

// SMI, HeapNumber
{
  const a = 7.1;
  const b = 9.9;
  test(17, a + b, true, 'Number');
  test(-18, a + b, false, 'Number');
}

test(18, NaN, false, 'Number');

// SMI, BigInt
testLoose(19, BigInt(19), true, 'Any');
testStrict(19, BigInt(19), false, 'Any');

// SMI, undetectable
test(0, %GetUndetectable(), false, 'Any');

// SMI, Oddball
testLoose(20, true, false, 'NumberOrBoolean');
testLoose(21, false, false, 'NumberOrBoolean');

// These should actually be NumberOrBoolean, but detecting it is not
// implemented (unclear if implementing it will improve performance).
testStrict(20, true, false, 'Any');
testStrict(21, false, false, 'Any');

testLoose(22, null, false, 'NumberOrOddball');
testStrict(22, null, false, 'Any');
testLoose(23, undefined, false, 'NumberOrOddball');
testStrict(23, undefined, false, 'Any');

// SMI, internalized string
testLoose(24, '24', true, 'Any');
testStrict(24, '24', false, 'Any');

// SMI, non-internalized string
{
  const a = '2';
  const b = '4';
  testLoose(24, a + b, true, 'Any');
  testStrict(24, a + b, false, 'Any');
}

// SMI, Symbol
test(25, Symbol('foo'), false, 'Any');

// SMI, Object
test(26, {}, false, 'Any');

// HeapNumber, HeapNumber
test(3.17, 3.17, true, 'Number');
test(-3.17, 3.17, false, 'Number');

// HeapNumber, NaN
test(3.18, NaN, false, 'Number');
test(NaN, NaN, false, 'Number');

// HeapNumber, BigInt
test(3.19, BigInt(319), false, 'Any');

// HeapNumber, undetectable
test(3.195, %GetUndetectable(), false, 'Any');

// HeapNumber, Oddball

testLoose(3.20, true, false, 'NumberOrBoolean');
testLoose(3.21, false, false, 'NumberOrBoolean');

// These should actually be NumberOrBoolean, but detecting it is not
// implemented (unclear if implementing it will improve performance).
testStrict(3.20, true, false, 'Any');
testStrict(3.21, false, false, 'Any');

testLoose(3.22, null, false, 'NumberOrOddball');
testStrict(3.22, null, false, 'Any');
testLoose(3.23, undefined, false, 'NumberOrOddball');
testStrict(3.23, undefined, false, 'Any');

// HeapNumber, internalized string
testLoose(3.24, '3.24', true, 'Any');
testStrict(3.24, '3.24', false, 'Any');

// HeapNumber, non-internalized string
{
  const a = '3.';
  const b = '25';
  testLoose(3.25, a + b, true, 'Any');
  testStrict(3.25, a + b, false, 'Any');
}

// HeapNumber, Symbol
test(3.26, Symbol('foo'), false, 'Any');

// HeapNumber, Object
test(3.27, {a: 5}, false, 'Any');

// BigInt, BigInt
test(BigInt(1000000000), BigInt(1000000000), true, 'BigInt(64)?');
test(BigInt(1000000000), BigInt(1000000001), false, 'BigInt(64)?');

// BigInt, undetectable
test(BigInt(999999999), %GetUndetectable(), false, 'Any');

// BigInt, Oddball
test(BigInt(1000000002), true, false, 'Any');
test(BigInt(1000000002), false, false, 'Any');

// TODO(397375000): Investigate this inconsistency. Should this be 'Any'?
testOneWay(BigInt(10000000002), null, '==', false, 'BigInt');
testOneWay(null, BigInt(10000000002), '==', false, 'Any');

testStrict(BigInt(1000000002), null, false, 'Any');

// TODO(397375000): Investigate this inconsistency. Should this be 'Any'?
testOneWay(BigInt(10000000002), undefined, '==', false, 'BigInt');
testOneWay(undefined, BigInt(10000000002), '==', false, 'Any');

testStrict(BigInt(1000000002), undefined, false, 'Any');

// BigInt, internalized string
testLoose(BigInt(1000000003), '1000000003', true, 'Any');
testStrict(BigInt(1000000003), '1000000003', false, 'Any');

// BigInt, non-internalized string
{
  const a = '100000000';
  const b = '4';
  testLoose(BigInt(1000000004), a + b, true, 'Any');
  testStrict(BigInt(1000000004), a + b, false, 'Any');
}

// BigInt, Symbol

// TODO(397375000): Investigate this inconsistency. Should this be 'Any'?
testOneWay(BigInt(1000000005), Symbol('s'), '==', false, 'BigInt');
testOneWay(Symbol('s'), BigInt(100000005), '==', false, 'Any');

testStrict(BigInt(1000000005), Symbol('s'), false, 'Any');

// BigInt, Object
test(BigInt(1000000006), {c: 16}, false, 'Any');

// undetectable, undetectable
test(%GetUndetectable(), %GetUndetectable(), false, 'Receiver');

// undetectable, Oddball
test(%GetUndetectable(), true, false, 'Any');
test(%GetUndetectable(), false, false, 'Any');
testLoose(%GetUndetectable(), null, true, 'ReceiverOrNullOrUndefined');
testStrict(%GetUndetectable(), null, false, 'ReceiverOrNullOrUndefined');
testLoose(%GetUndetectable(), undefined, true, 'ReceiverOrNullOrUndefined');
testStrict(%GetUndetectable(), undefined, false, 'ReceiverOrNullOrUndefined');

// undetectable, internalized string
test(%GetUndetectable(), '1000000003', false, 'Any');

// undetectable, non-internalized string
{
  const a = 'foo';
  const b = 'bar';
  test(%GetUndetectable(), a + b, false, 'Any');
}

// undetectable, Symbol
test(%GetUndetectable(), Symbol('s'), false, 'Any');

// undetectable, Object
test(%GetUndetectable(), {c: 16}, false, 'Receiver');

// Oddball, Oddball
test(true, true, true, 'NumberOrBoolean');
test(false, false, true, 'NumberOrBoolean');

// TODO(397375000): This should be NumberOrBoolean too.
testStrict(true, false, false, 'Any');

testLoose(true, false, false, 'NumberOrBoolean');

// TODO(397375000): These should be NumberOrOddball.
testOneWay(true, null, '===', false, 'Any');
testOneWay(null, true, '===', false, 'ReceiverOrNullOrUndefined');
testOneWay(true, undefined, '===', false, 'Any');
testOneWay(undefined, true, '===', false, 'ReceiverOrNullOrUndefined');
testOneWay(false, null, '===', false, 'Any');
testOneWay(null, false, '===', false, 'ReceiverOrNullOrUndefined');
testOneWay(false, undefined, '===', false, 'Any');
testOneWay(undefined, false, '===', false, 'ReceiverOrNullOrUndefined');

// TODO(397375000): These should be NumberOrOddball.
testOneWay(true, null, '==', false, 'NumberOrOddball');
testOneWay(null, true, '===', false, 'ReceiverOrNullOrUndefined');
testLoose(true, undefined, false, 'NumberOrOddball');
testLoose(false, null, false, 'NumberOrOddball');
testLoose(false, undefined, false, 'NumberOrOddball');

test(undefined, undefined, true, 'ReceiverOrNullOrUndefined');
testLoose(undefined, null, true, 'ReceiverOrNullOrUndefined');
testStrict(undefined, null, false, 'ReceiverOrNullOrUndefined');
test(null, null, true, 'ReceiverOrNullOrUndefined');

// Oddball, internalized string
test(true, 'true', false, 'Any');
testLoose(false, '', true, 'Any');
testStrict(false, '', false, 'Any');
test(undefined, '', false, 'Any');
test(null, '', false, 'Any');

// Oddball, non-internalized string
{
  const a = '100000000';
  const b = '4';
  test(true, a + b, false, 'Any');
  test(false, a + b, false, 'Any');
  test(undefined, a + b, false, 'Any');
  test(null, a + b, false, 'Any');
}

// Oddball, Symbol
test(true, Symbol('true'), false, 'Any');
test(false, Symbol(), false, 'Any');
test(undefined, Symbol(), false, 'Any');
test(null, Symbol(), false, 'Any');

// Oddball, Object
test(true, {a: 'b'}, false, 'Any');
test(false, {a: 'b'}, false, 'Any');
test(undefined, {a: 'b'}, false, 'ReceiverOrNullOrUndefined');
test(null, {a: 'b'}, false, 'ReceiverOrNullOrUndefined');

{
  // Internalized String, internalized String
  test('internalized', 'internalized', true, 'InternalizedString');
  test('not', 'equal', false, 'InternalizedString');

  // Internalized string, non-internalized string
  const a = 'non';
  const b = '-internalized';
  test('non-internalized', a + b, true, 'String');
  test('not equal', a + b, false, 'String');

  // Non-internalized string, non-internalized string
  const c = 'non-in';
  const d = 'ternalized';
  test(a + b, c + d, true, 'String');
  test(a + b + b, c + d, false, 'String');

  // Internalized string, Symbol
  test('string', Symbol('string'), false, 'Any');

  // Non-internalized string, Symbol
  test(a + b, Symbol('non-internalized'), false, 'Any');

  // Internalized string, Object
  test('string', {}, false, 'Any');

  // Non-internalized string, Object
  test(a + b, {}, false, 'Any');
}

// Symbol, Symbol
test(Symbol('private'), Symbol('private'), false, 'Symbol');
test(Symbol('something'), Symbol.for('something'), false, 'Symbol');
test(Symbol.for('something'), Symbol.for('something'), true, 'Symbol');

// Symbol, Object
test(Symbol('private'), {}, false, 'Any');

// Object, Object
test({}, {}, false, 'Receiver');
{
  const a = {};
  test(a, a, true, 'Receiver');
}
