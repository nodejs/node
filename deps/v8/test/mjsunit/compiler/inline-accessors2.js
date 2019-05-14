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

// Flags: --allow-natives-syntax --inline-accessors

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

function TryGetter(context, getter, obj, expected, expectException) {
  try {
    TestInlinedGetter(context, obj, expected);
    assertFalse(expectException);
  } catch (exception) {
    assertTrue(expectException);
    assertEquals(7, exception.stack.split('\n').length);
  }
  %DeoptimizeFunction(context);
  %ClearFunctionFeedback(context);
  %ClearFunctionFeedback(getter);
}

function TestGetterInAllContexts(getter, obj, expected, expectException) {
  TryGetter(value_context_for_getter, getter, obj, expected, expectException);
  TryGetter(test_context_for_getter, getter, obj, expected ? 111 : 222,
            expectException);
  TryGetter(effect_context_for_getter, getter, obj, 5678, expectException);
}

// -----------------------------------------------------------------------------
// Test getter returning something 'true'ish in all contexts.

function getter1() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  return 1234;
}

function ConstrG1() { }
obj = Object.defineProperty(new ConstrG1(), "getterProperty", { get: getter1 });
TestGetterInAllContexts(getter1, obj, 1234, false);
obj = Object.create(obj);
TestGetterInAllContexts(getter1, obj, 1234, false);

// -----------------------------------------------------------------------------
// Test getter returning false in all contexts.

function getter2() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  return false;
}

function ConstrG2() { }
obj = Object.defineProperty(new ConstrG2(), "getterProperty", { get: getter2 });
TestGetterInAllContexts(getter2, obj, false, false);
obj = Object.create(obj);
TestGetterInAllContexts(getter2, obj, false, false);

// -----------------------------------------------------------------------------
// Test getter without a return in all contexts.

function getter3() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
}

function ConstrG3() { }
obj = Object.defineProperty(new ConstrG3(), "getterProperty", { get: getter3 });
TestGetterInAllContexts(getter3, obj, undefined, false);
obj = Object.create(obj);
TestGetterInAllContexts(getter3, obj, undefined, false);

// -----------------------------------------------------------------------------
// Test getter with too many arguments without a return in all contexts.

function getter4(a) {
  assertSame(obj, this);
  assertEquals(undefined, a);
  accessorCallCount++;
  forceDeopt.deopt;
}

function ConstrG4() { }
obj = Object.defineProperty(new ConstrG4(), "getterProperty", { get: getter4 });
TestGetterInAllContexts(getter4, obj, undefined, false);
obj = Object.create(obj);
TestGetterInAllContexts(getter4, obj, undefined, false);

// -----------------------------------------------------------------------------
// Test getter with too many arguments with a return in all contexts.

function getter5(a) {
  assertSame(obj, this);
  assertEquals(undefined, a);
  accessorCallCount++;
  forceDeopt.deopt;
  return 9876;
}

function ConstrG5() { }
obj = Object.defineProperty(new ConstrG5(), "getterProperty", { get: getter5 });
TestGetterInAllContexts(getter5, obj, 9876, false);
obj = Object.create(obj);
TestGetterInAllContexts(getter5, obj, 9876, false);

// -----------------------------------------------------------------------------
// Test getter which throws from optimized code.

function getter6() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  if (accessorCallCount == 4) { 123 in null; }
  return 13579;
}

function ConstrG6() { }
obj = Object.defineProperty(new ConstrG6(), "getterProperty", { get: getter6 });
TestGetterInAllContexts(getter6, obj, 13579, true);
obj = Object.create(obj);
TestGetterInAllContexts(getter6, obj, 13579, true);

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

function TrySetter(context, setter, obj, expectException, value, expected) {
  try {
    TestInlinedSetter(context, obj, value, expected);
    assertFalse(expectException);
  } catch (exception) {
    assertTrue(expectException);
    assertEquals(7, exception.stack.split('\n').length);
  }
  %DeoptimizeFunction(context);
  %ClearFunctionFeedback(context);
  %ClearFunctionFeedback(setter);
}

function TestSetterInAllContexts(setter, obj, expectException) {
  TrySetter(value_context_for_setter, setter, obj, expectException, 111, 111);
  TrySetter(test_context_for_setter, setter, obj, expectException, true, 333);
  TrySetter(test_context_for_setter, setter, obj, expectException, false, 444);
  TrySetter(effect_context_for_setter, setter, obj, expectException, 555, 666);
}

// -----------------------------------------------------------------------------
// Test setter without a return in all contexts.

function setter1(value) {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = value;
}

function ConstrS1() { }
obj = Object.defineProperty(new ConstrS1(), "setterProperty", { set: setter1 });
TestSetterInAllContexts(setter1, obj, false);
obj = Object.create(obj);
TestSetterInAllContexts(setter1, obj, false);

// -----------------------------------------------------------------------------
// Test setter returning something different than the RHS in all contexts.

function setter2(value) {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = value;
  return 1000000;
}

function ConstrS2() { }
obj = Object.defineProperty(new ConstrS2(), "setterProperty", { set: setter2 });
TestSetterInAllContexts(setter2, obj, false);
obj = Object.create(obj);
TestSetterInAllContexts(setter2, obj, false);

// -----------------------------------------------------------------------------
// Test setter with too few arguments without a return in all contexts.

function setter3() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = setterArgument;
}

function ConstrS3() { }
obj = Object.defineProperty(new ConstrS3(), "setterProperty", { set: setter3 });
TestSetterInAllContexts(setter3, obj, false);
obj = Object.create(obj);
TestSetterInAllContexts(setter3, obj, false);

// -----------------------------------------------------------------------------
// Test setter with too few arguments with a return in all contexts.

function setter4() {
  assertSame(obj, this);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = setterArgument;
  return 2000000;
}

function ConstrS4() { }
obj = Object.defineProperty(new ConstrS4(), "setterProperty", { set: setter4 });
TestSetterInAllContexts(setter4, obj, false);
obj = Object.create(obj);
TestSetterInAllContexts(setter4, obj, false);

// -----------------------------------------------------------------------------
// Test setter with too many arguments without a return in all contexts.

function setter5(value, foo) {
  assertSame(obj, this);
  assertEquals(undefined, foo);
  accessorCallCount++;
  forceDeopt.deopt;
  setterValue = value;
}

function ConstrS5() { }
obj = Object.defineProperty(new ConstrS5(), "setterProperty", { set: setter5 });
TestSetterInAllContexts(setter5, obj, false);
obj = Object.create(obj);
TestSetterInAllContexts(setter5, obj, false);

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

function ConstrS6() { }
obj = Object.defineProperty(new ConstrS6(), "setterProperty", { set: setter6 });
TestSetterInAllContexts(setter6, obj, false);
obj = Object.create(obj);
TestSetterInAllContexts(setter6, obj, false);

// -----------------------------------------------------------------------------
// Test setter which throws from optimized code.

function setter7(value) {
  accessorCallCount++;
  forceDeopt.deopt;
  if (accessorCallCount == 4) { 123 in null; }
  setterValue = value;
}

function ConstrS7() { }
obj = Object.defineProperty(new ConstrS7(), "setterProperty", { set: setter7 });
TestSetterInAllContexts(setter7, obj, true);
obj = Object.create(obj);
TestSetterInAllContexts(setter7, obj, true);
