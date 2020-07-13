// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-promise-any

(function TestNoParameters() {
    // Can't omit the "errors" parameter; there's nothing to iterate.
    assertThrows(() => { new AggregateError(); });
})();

(function TestNoParameters_NoNew() {
    // Can't omit the "errors" parameter; there's nothing to iterate.
    assertThrows(() => { AggregateError(); });
})();

(function TestOneParameterErrorsIsArray() {
    let error = new AggregateError([1, 20, 4]);
    assertEquals('', error.message);
    assertEquals([1, 20, 4], error.errors);
})();

(function TestOneParameterErrorsIsArray_NoNew() {
    let error = AggregateError([1, 20, 4]);
    assertEquals('', error.message);
    assertEquals([1, 20, 4], error.errors);
})();

(function TestOneParameterErrosIsAnEmptyArray() {
    let error = new AggregateError([]);
    assertEquals('', error.message);
    assertEquals([], error.errors);
})();

(function TestOneParameterErrorsIsASet() {
    let set = new Set();
    set.add(5);
    set.add(100);
    let error = new AggregateError(set);
    assertEquals('', error.message);
    assertEquals(2, error.errors.length);
    assertTrue(error.errors[0] == 5 || error.errors[1] == 5);
    assertTrue(error.errors[0] == 100 || error.errors[1] == 100);
})();

(function TestOneParameterErrorsNotIterable() {
    assertThrows(() => { new AggregateError(5); });
})();

(function TestOneParameterErrorsNotIterable_NoNew() {
    assertThrows(() => { AggregateError(5); });
})();

(function TestTwoParameters() {
    let error = new AggregateError([1, 20, 4], 'custom message');
    assertEquals('custom message', error.message);
    assertEquals([1, 20, 4], error.errors);
})();

(function TestTwoParameters_NoNew() {
    let error = AggregateError([1, 20, 4], 'custom message');
    assertEquals('custom message', error.message);
    assertEquals([1, 20, 4], error.errors);
})();

(function TestTwoParametersMessageNotString() {
    let custom = { toString() { return 'hello'; } };
    let error = new AggregateError([], custom);
    assertEquals('hello', error.message);
})();

(function TestTwoParametersMessageIsSMI() {
    let error = new AggregateError([], 44);
    assertEquals('44', error.message);
})();

(function TestTwoParametersMessageUndefined() {
    let error = new AggregateError([], undefined);
    assertFalse(Object.prototype.hasOwnProperty.call(error, 'message'));
})();

(function AggregateErrorPrototypeErrorsCalledWithWrongTypeOfObject() {
    let f = Object.getOwnPropertyDescriptor(AggregateError.prototype, 'errors').get;

    // This works:
    let error = new AggregateError([3]);
    let got_errors = f.call(error);
    assertEquals([3], got_errors);

    // This doesn't:
    assertThrows(() => { f.call({}) } );
})();

(function AggregateErrorPrototypeErrorsCalledWithTooManyArguments() {
    let f = Object.getOwnPropertyDescriptor(AggregateError.prototype, 'errors').get;
    let error = new AggregateError([3]);
    let got_errors = f.call(error, ["unnecessary", "arguments"]);
    assertEquals([3], got_errors);
})();

(function SetErrorsSloppy() {
  let e = new AggregateError([1]);
  e.errors = [4, 5, 6];
  assertEquals([1], e.errors);
})();

(function SetErrorsStrict() {
    "use strict";
    let e = new AggregateError([1]);
    assertThrows(() => { e.errors = [4, 5, 6];});
})();

(function SubClassProto() {
    class A extends AggregateError {
        constructor() {
            super([]);
        }
    }

    let o = new A();
    assertEquals(o.__proto__, A.prototype);
})();

(function ErrorsWithHoles() {
    let errors = [0];
    errors[2] = 2;
    let a = new AggregateError(errors);
    assertEquals([0, undefined, 2], a.errors);
})();

(function ErrorsIsANewArray(){
  let array = [8, 9];
  let e = new AggregateError(array);
  array.push(1);
  assertEquals([8, 9], e.errors);
})();

(function ErrorsIsANewArrayForEachGetterCall(){
    let e = new AggregateError([9, 6, 3]);
    const errors1 = e.errors;
    const errors2 = e.errors;
    assertNotSame(errors1, errors2);
})();

(function ErrorsModified(){
    let e = new AggregateError([9, 6, 3]);
    const errors1 = e.errors;
    errors1[0] = 50;
    const errors2 = e.errors;
    assertEquals([50, 6, 3], errors1);
    assertEquals([9, 6, 3], errors2);
})();

(function EmptyErrorsModified1(){
    let e = new AggregateError([]);
    const errors1 = e.errors;
    errors1[0] = 50;
    const errors2 = e.errors;
    assertEquals([50], errors1);
    assertEquals([], errors2);
})();

(function EmptyErrorsModified2(){
    let e = new AggregateError([]);
    const errors1 = e.errors;
    errors1.push(50);
    const errors2 = e.errors;
    assertEquals([50], errors1);
    assertEquals([], errors2);
})();

(function AggregateErrorCreation() {
    // Verify that we match the spec wrt getting the prototype from the
    // newTarget, iterating the errors array and calling toString on the
    // message.
    let counter = 1;
    let prototype_got = 0;
    let errors_iterated = 0;
    let to_string_called = 0;

    // For observing Get(new target, "prototype")
    function target() {}
    let handler = {
        get: (target, prop, receiver) => {
          if (prop == 'prototype') {
            prototype_got = counter++;
            return target.prototype;
          }
        }
    };
    let p = new Proxy(target, handler);

    // For observing IterableToList(errors)
    var errors = {
      [Symbol.iterator]() {
        return {
          next() {
            errors_iterated = counter++;
            return { done: true };
          }
        };
      }
    };

    // For observing ToString(message)
    let message = { toString: () => { to_string_called = counter++;}}

    let o = Reflect.construct(AggregateError, [errors, message], p);

    assertEquals(1, prototype_got);
    assertEquals(2, to_string_called);
    assertEquals(3, errors_iterated);
 })();
