// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that asynchronous features do not work with
// side-effect free debug-evaluate.

Debug = debug.Debug

var exception = null;

function* generator() {
  yield 1;
}

async function async() {
  return 1;
}

var g = generator();

var p = new Promise(() => {});

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    function fail(source) {
      assertThrows(() => exec_state.frame(0).evaluate(source, true),
                   EvalError);
    }
    fail("new Promise()");
    fail("generator()");
    fail("g.next()");
    fail("async()");
    fail("Promise.resolve()");
    fail("Promise.reject()");
    fail("p.then(() => {})");
    fail("p.catch(() => {})");
    fail("p.finally(() => {})");
    fail("Promise.all([p, p])");
    fail("Promise.race([p, p])");
    fail("(async function() {})()");
    fail("(async function() { await 1; })()");
  } catch (e) {
    exception = e;
    print(e, e.stack);
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  debugger;
};

f();

assertNull(exception);
