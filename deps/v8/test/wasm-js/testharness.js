// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementations of some functions from testharness.js
// See https://github.com/web-platform-tests/wpt/blob/master/resources/testharness.js
// Licensed as follows:
//
// Distributed under both the W3C Test Suite License [1] and the W3C
// 3-clause BSD License [2]. To contribute to a W3C Test Suite, see the
// policies and contribution forms [3].
// [1] http://www.w3.org/Consortium/Legal/2008/04-testsuite-license
// [2] http://www.w3.org/Consortium/Legal/2008/03-bsd-license
// [3] http://www.w3.org/2004/10/27-testcases

const failures = [];

let lastPromise = Promise.resolve();

function test(func, description) {
  let maybeErr;
  try { func({unreached_func: unreached_func}); }
  catch(e) { maybeErr = e; }
  if (typeof maybeErr !== 'undefined') {
    console.log(`${description}: FAIL. ${maybeErr}`);
    failures.push(description);
  } else {
    console.log(`${description}: PASS.`);
  }
}

function promise_test(func, description) {
  lastPromise = lastPromise.then(func)
  .then(_ => {
    console.log(`${description}: PASS.`);
  })
  .catch(err => {
    console.log(`${description}: FAIL. ${err}`);
    failures.push(description);
  });
}

const assert_true = assertEquals.bind(null, true);
const assert_false = assertEquals.bind(null, false);

function same_value(x, y) {
  if (y !== y) {
    // NaN case
    return x!==x;
  }
  if (x === 0 && y === 0) {
    // Distinguish +0 and -0
    return 1/x === 1/y;
  }
  return x === y;
}

function assert_equals(expected, found, description) {
  if (typeof found != typeof expected) {
    assert_true(false, "assert_equals", description,
        "expected (" + typeof expected + ") ${expected} but got (" +
        typeof found + ") ${found}", {expected:expected, found:found});
  }
  assert_true(same_value(found, expected), "assert_equals", description,
      "expected ${expected} but got ${found}",
      {expected:expected, found:found});
}

function assert_not_equals(expected, found, description) {
  assert_true(!same_value(found, expected), "assert_not_equals", description,
      "got disallowed value ${found}", {found:found});
}

function assert_array_equals(actual, expected, description) {
  assert_true(
      typeof actual === 'object' && actual !== null && 'length' in actual,
      'assert_array_equals', description, 'value is ${actual}, expected array',
      {actual: actual});
  assert_true(
      actual.length === expected.length, 'assert_array_equals', description,
      'lengths differ, expected ${expected} got ${actual}',
      {expected: expected.length, actual: actual.length});

  for (let i = 0; i < actual.length; i++) {
    assert_true(
        actual.hasOwnProperty(i) === expected.hasOwnProperty(i),
        'assert_array_equals', description,
        'property ${i}, property expected to be ${expected} but was ${actual}',
        {
          i: i,
          expected: expected.hasOwnProperty(i) ? 'present' : 'missing',
          actual: actual.hasOwnProperty(i) ? 'present' : 'missing'
        });
    assert_true(
        same_value(expected[i], actual[i]), 'assert_array_equals', description,
        'property ${i}, expected ${expected} but got ${actual}',
        {i: i, expected: expected[i], actual: actual[i]});
  }
}

function unreached_func(msg) {
  return function trap() {
    assert_unreached(msg);
  };
}

function assert_unreached(description) {
  throw new Error(`unreachable:\n${description}`);
}

function format_value(s) {
  // TODO
  try {
    return String(s);
  } catch(e) {
    return `<String(e) for type ${typeof(e)} threw>`;
  }
}

function promise_rejects(test, expected, promise, description) {
  return promise
      .then(() => assert_unreached('Should have rejected: ' + description))
      .catch(function(e) {
        assert_throws(expected, function() {
          throw e;
        }, description);
      });
}

function assert_class_string(object, class_string, description) {
  assert_equals(
      {}.toString.call(object), '[object ' + class_string + ']', description);
}

function assert_throws(code, func, description) {
  try {
    func();
  } catch (e) {
    assert_true(e.name === code.name, "expected exception " + code.name + ", got " + e.name);
    return;
  }
  assert_true(false, "expected exception " + code.name + ", no exception thrown");
}

function setup(func) {
  // TODO need to do anything fancier here?
  func();
}
