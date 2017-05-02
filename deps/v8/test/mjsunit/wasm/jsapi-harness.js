// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(titzer): update spec test suite to version 0x1
if (false) {

// TODO(eholk): Once we have stable test IDs, use those as the key instead.
// See https://github.com/WebAssembly/spec/issues/415
const known_failures = {
  "'WebAssembly.Module.customSections' method":
    'https://bugs.chromium.org/p/v8/issues/detail?id=5815',
  "'WebAssembly.Table.prototype.get' method":
    'https://bugs.chromium.org/p/v8/issues/detail?id=5507',
  "'WebAssembly.Table.prototype.set' method":
    'https://bugs.chromium.org/p/v8/issues/detail?id=5507',
};

let failures = [];

let last_promise = new Promise((resolve, reject) => { resolve(); });

function test(func, description) {
  let maybeErr;
  try { func(); }
  catch(e) { maybeErr = e; }
  if (typeof maybeErr !== 'undefined') {
    print(`${description}: FAIL. ${maybeErr}`);
    failures.push(description);
  } else {
    print(`${description}: PASS.`);
  }
}

function promise_test(func, description) {
  last_promise = last_promise.then(func)
  .then(_ => { print(`${description}: PASS.`); })
  .catch(err => {
    print(`${description}: FAIL. ${err}`);
    failures.push(description);
  });
}

let assert_equals = assertEquals;
let assert_true = assertEquals.bind(null, true);
let assert_false = assertEquals.bind(null, false);

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

last_promise.then(_ => {
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
    if (unexpected) {
      quit(1);
    }
  }
});

}
