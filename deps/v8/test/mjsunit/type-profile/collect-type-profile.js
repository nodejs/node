// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --type-profile --turbo --allow-natives-syntax

function check_collect_types(name, expected) {
  const type_profile = %TypeProfile(name);
  if (type_profile !== undefined) {
    const result = JSON.stringify(type_profile);
    print(result);
    assertEquals(expected, result, name + " failed");

  }
}

function testFunction(param, flag) {
  // We want to test 2 different return positions in one function.
  if (flag) {
    var first_var = param;
    return first_var;
  }
  var second_var = param;
  return second_var;
}

class MyClass {
  constructor() {}
}

var expected = `{}`;
check_collect_types(testFunction, expected);

testFunction({});
testFunction(123, true);
testFunction('hello');
testFunction(123);

expected = `{\"503\":[\"Object\",\"number\",\"string\",\"number\"],\"510\":[\"undefined\",\"boolean\",\"undefined\",\"undefined\"],\"699\":[\"Object\",\"number\",\"string\",\"number\"]}`;
check_collect_types(testFunction, expected);

testFunction(undefined);
testFunction('hello', true);
testFunction({x: 12}, true);
testFunction({x: 12});
testFunction(new MyClass());

expected = `{\"503\":[\"Object\",\"number\",\"string\",\"number\",\"undefined\",\"string\",\"Object\",\"Object\",\"MyClass\"],\"510\":[\"undefined\",\"boolean\",\"undefined\",\"undefined\",\"undefined\",\"boolean\",\"boolean\",\"undefined\",\"undefined\"],\"699\":[\"Object\",\"number\",\"string\",\"number\",\"undefined\",\"string\",\"Object\",\"Object\",\"MyClass\"]}`;
check_collect_types(testFunction, expected);


function testReturnOfNonVariable() {
  return 32;
}
testReturnOfNonVariable();
expected = `{\"1732\":[\"number\"]}`;
check_collect_types(testReturnOfNonVariable, expected);

// Return statement is reached but its expression is never really returned.
function try_finally() {
  try {
    return 23;
  } finally {
    return "nope, string is better"
  }
}
try_finally();
expected = `{\"2034\":[\"string\"]}`;
check_collect_types(try_finally, expected);

// Fall-off return.
function fall_off() {
  //nothing
}
fall_off();
expected = `{\"2188\":[\"undefined\"]}`;
check_collect_types(fall_off, expected);

// Do not collect types when the function is never run.
function never_called() {}
expected = `{}`;
check_collect_types(never_called, expected);


function several_params(a, b, c, d) {
  //nothing
}
several_params(2, 'foo', {}, new MyClass());
expected = `{\"2456\":[\"number\"],\"2459\":[\"string\"],\"2462\":[\"Object\"],\"2465\":[\"MyClass\"],\"2482\":[\"undefined\"]}`;
check_collect_types(several_params, expected);
