// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(eholk): Once we have stable test IDs, use those as the key instead.
// See https://github.com/WebAssembly/spec/issues/415
//
// Flags: --expose-wasm --allow-natives-syntax

const known_failures = {
  // Enter failing tests like follows:
  // "'WebAssembly.Instance.prototype.exports' accessor property":
  //  'https://bugs.chromium.org/p/v8/issues/detail?id=5507',
};

let failures = [];
let unexpected_successes = [];

let last_promise = new Promise((resolve, reject) => { resolve(); });

function test(func, description) {
  let maybeErr;
  try { func(); }
  catch(e) { maybeErr = e; }
  if (typeof maybeErr !== 'undefined') {
    var known = "";
    if (known_failures[description]) {
      known = " (known)";
    }
    print(`${description}: FAIL${known}. ${maybeErr}`);
    failures.push(description);
  } else {
    if (known_failures[description]) {
      unexpected_successes.push(description);
    }
    print(`${description}: PASS.`);
  }
}

function promise_test(func, description) {
  last_promise = last_promise.then(func)
  .then(_ => {
    if (known_failures[description]) {
      unexpected_successes.push(description);
    }
    print(`${description}: PASS.`);
  })
  .catch(err => {
    var known = "";
    if (known_failures[description]) {
      known = " (known)";
    }
    print(`${description}: FAIL${known}. ${err}`);
    failures.push(description);
  });
}

let assert_true = assertEquals.bind(null, true);
let assert_false = assertEquals.bind(null, false);

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

let assert_equals = function(expected, found, description) {
  if (typeof found != typeof expected) {
    assert_true(false, "assert_equals", description,
        "expected (" + typeof expected + ") ${expected} but got (" +
        typeof found + ") ${found}", {expected:expected, found:found});
  }
  assert_true(same_value(found, expected), "assert_equals", description,
      "expected ${expected} but got ${found}",
      {expected:expected, found:found});
}

let assert_not_equals = function(expected, found, description) {
  assert_true(!same_value(found, expected), "assert_not_equals", description,
      "got disallowed value ${found}", {found:found});
}

function assert_unreached(description) {
  throw new Error(`unreachable:\n${description}`);
}

function assertErrorMessage(f, ctor, test) {
  try { f(); }
  catch (e) {
    assert_true(e instanceof ctor, "expected exception " + ctor.name + ", got " + e);
    return;
  }
  assert_true(false, "expected exception " + ctor.name + ", no exception thrown");
};

load("test/wasm-js/test/harness/wasm-constants.js");
load("test/wasm-js/test/harness/wasm-module-builder.js");
load("test/wasm-js/test/js-api/jsapi.js");

assertPromiseResult(last_promise, _ => {
  if (failures.length > 0) {
    let unexpected = false;
    print("Some tests FAILED:");
    for (let i in failures) {
      if (known_failures[failures[i]]) {
        print(`  ${failures[i]} [KNOWN: ${known_failures[failures[i]]}]`);
      } else {
        print(`  ${failures[i]}`);
        unexpected = true;
      }
    }
    if (unexpected_successes.length > 0) {
      unexpected = true;
      print("");
      print("Unexpected successes:");
      for(let i in unexpected_successes) {
        print(`  ${unexpected_successes[i]}`);
      }
      print("Some tests SUCCEEDED but were known failures. If you've fixed " +
            "the bug, please remove the test from the known failures list.")
    }
    if (unexpected) {
      print("\n");
      print("   #############################################################");
      print("   #                                                           #");
      print("   # Unexpected outcome. Did you forget to run 'gclient sync'? #");
      print("   #                                                           #");
      print("   #############################################################");
      print("\n");
      assertUnreachable("Unexpected outcome");
    }
  }
});
