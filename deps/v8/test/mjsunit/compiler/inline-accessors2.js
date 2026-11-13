// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax

var accessorCallCount, setterArgument, setterValue, obj, forceDeopt;

// -----------------------------------------------------------------------------
// Helpers for testing inlining of getters.

function TestInlinedGetter(context, obj, expected) {
  forceDeopt = { deopt: 0 };
  accessorCallCount = 0;

  %PrepareFunctionForOptimization(context);
  assertEquals(expected, context(obj));
  assertEquals(1, accessorCallCount);

  assertEquals(expected, context(obj));
  assertEquals(2, accessorCallCount);

  %OptimizeFunctionOnNextCall(context);
  assertEquals(expected, context(obj));
  assertEquals(3, accessorCallCount);

  forceDeopt = { /* empty*/ };
  assertEquals(expected, context(obj));
  assertEquals(4, accessorCallCount);
}


function value_context_for_getter(obj) {
  return obj.getterProperty;
}

function test_context_for_getter(obj) {
  if (obj.getterProperty) {
    return 111;
  } else {
    return 222;
  }
}

function effect_context_for_getter(obj) {
  obj.getterProperty;
  return 5678;
}

function Constr_template() {}

function TryGetter(context, getter, isProto, expected, expectException) {
  context = copyFunctionWithFreshFeedback(context);
  getter = copyFunctionWithFreshFeedback(getter);
  %PrepareFunctionForOptimization(getter);
  // Construct a fresh holder from a fresh constructor so defining the fresh
  // `getter` closure keeps `holder` in fast mode instead of normalizing its map
  // via AccessorsOverwritingAccessors / TransitionToDifferentAccessor.
  var Constr = copyFunctionWithFreshFeedback(Constr_template);
  var holder = Object.defineProperty(
      new Constr(), "getterProperty", { get: getter });
  obj = isProto ? Object.create(holder) : holder;
  try {
    TestInlinedGetter(context, obj, expected);
    assertFalse(expectException);
  } catch (exception) {
    assertTrue(expectException);
    assertEquals(7, exception.stack.split('\n').length);
  }
  %DeoptimizeFunction(context);
}

function TestGetterInAllContexts(getter, expected, expectException) {
  for (var isProto of [false, true]) {
    TryGetter(value_context_for_getter, getter, isProto, expected,
              expectException);
    TryGetter(test_context_for_getter, getter, isProto, expected ? 111 : 222,
              expectException);
    TryGetter(effect_context_for_getter, getter, isProto, 5678,
              expectException);
  }
}

// -----------------------------------------------------------------------------
// Test getter returning something 'true'ish in all contexts.

function getter1() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  return 1234;
}

TestGetterInAllContexts(getter1, 1234, false);

// -----------------------------------------------------------------------------
// Test getter returning false in all contexts.

function getter2() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  return false;
}

TestGetterInAllContexts(getter2, false, false);

// -----------------------------------------------------------------------------
// Test getter without a return in all contexts.

function getter3() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
}

TestGetterInAllContexts(getter3, undefined, false);

// -----------------------------------------------------------------------------
// Test getter with too many arguments without a return in all contexts.

function getter4(a) {
  assertSame(obj, this);
  assertEquals(undefined, a);
  accessorCallCount++;
  forceDeopt.deopt;
}

TestGetterInAllContexts(getter4, undefined, false);

// -----------------------------------------------------------------------------
// Test getter with too many arguments with a return in all contexts.

function getter5(a) {
  assertSame(obj, this);
  assertEquals(undefined, a);
  accessorCallCount++;
  forceDeopt.deopt;
  return 9876;
}

TestGetterInAllContexts(getter5, 9876, false);

// -----------------------------------------------------------------------------
// Test getter which throws from optimized code.

function getter6() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  if (accessorCallCount == 4) { 123 in null; }
  return 13579;
}

TestGetterInAllContexts(getter6, 13579, true);

// -----------------------------------------------------------------------------
// Helpers for testing inlining of setters.

function TestInlinedSetter(context, obj, value, expected) {
  forceDeopt = { deopt: 0 };
  accessorCallCount = 0;
  setterArgument = value;

  %PrepareFunctionForOptimization(context);
  assertEquals(expected, context(obj, value));
  assertEquals(value, setterValue);
  assertEquals(1, accessorCallCount);

  assertEquals(expected, context(obj, value));
  assertEquals(value, setterValue);
  assertEquals(2, accessorCallCount);

  %OptimizeFunctionOnNextCall(context);
  assertEquals(expected, context(obj, value));
  assertEquals(value, setterValue);
  assertEquals(3, accessorCallCount);

  forceDeopt = { /* empty*/ };
  assertEquals(expected, context(obj, value));
  assertEquals(value, setterValue);
  assertEquals(4, accessorCallCount);
}

function value_context_for_setter(obj, value) {
  return obj.setterProperty = value;
}

function test_context_for_setter(obj, value) {
  if (obj.setterProperty = value) {
    return 333;
  } else {
    return 444;
  }
}

function effect_context_for_setter(obj, value) {
  obj.setterProperty = value;
  return 666;
}

function TrySetter(context, setter, isProto, expectException, value, expected) {
  context = copyFunctionWithFreshFeedback(context);
  setter = copyFunctionWithFreshFeedback(setter);
  %PrepareFunctionForOptimization(setter);
  // Construct a fresh holder from a fresh constructor so defining the fresh
  // `setter` closure keeps `holder` in fast mode instead of normalizing its map
  // via AccessorsOverwritingAccessors / TransitionToDifferentAccessor.
  var Constr = copyFunctionWithFreshFeedback(Constr_template);
  var holder = Object.defineProperty(
      new Constr(), "setterProperty", { set: setter });
  obj = isProto ? Object.create(holder) : holder;
  try {
    TestInlinedSetter(context, obj, value, expected);
    assertFalse(expectException);
  } catch (exception) {
    assertTrue(expectException);
    assertEquals(7, exception.stack.split('\n').length);
  }
  %DeoptimizeFunction(context);
}

function TestSetterInAllContexts(setter, expectException) {
  for (var isProto of [false, true]) {
    TrySetter(value_context_for_setter, setter, isProto, expectException, 111,
              111);
    TrySetter(test_context_for_setter, setter, isProto, expectException, true,
              333);
    TrySetter(test_context_for_setter, setter, isProto, expectException, false,
              444);
    TrySetter(effect_context_for_setter, setter, isProto, expectException, 555,
              666);
  }
}

// -----------------------------------------------------------------------------
// Test setter without a return in all contexts.

function setter1(value) {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = value;
}

TestSetterInAllContexts(setter1, false);

// -----------------------------------------------------------------------------
// Test setter returning something different than the RHS in all contexts.

function setter2(value) {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = value;
  return 1000000;
}

TestSetterInAllContexts(setter2, false);

// -----------------------------------------------------------------------------
// Test setter with too few arguments without a return in all contexts.

function setter3() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = setterArgument;
}

TestSetterInAllContexts(setter3, false);

// -----------------------------------------------------------------------------
// Test setter with too few arguments with a return in all contexts.

function setter4() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = setterArgument;
  return 2000000;
}

TestSetterInAllContexts(setter4, false);

// -----------------------------------------------------------------------------
// Test setter with too many arguments without a return in all contexts.

function setter5(value, foo) {
  assertSame(obj, this);
  assertEquals(undefined, foo);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = value;
}

TestSetterInAllContexts(setter5, false);

// -----------------------------------------------------------------------------
// Test setter with too many arguments with a return in all contexts.

function setter6(value, foo) {
  assertSame(obj, this);
  assertEquals(undefined, foo);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = value;
  return 3000000;
}

TestSetterInAllContexts(setter6, false);

// -----------------------------------------------------------------------------
// Test setter which throws from optimized code.

function setter7(value) {
  accessorCallCount++;
  forceDeopt.deopt;
  if (accessorCallCount == 4) { 123 in null; }
  setterValue = value;
}

TestSetterInAllContexts(setter7, true);
