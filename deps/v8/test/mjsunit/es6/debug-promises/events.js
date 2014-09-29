// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-debug-as debug

Debug = debug.Debug;

var eventsExpected = 16;
var exception = null;
var result = [];

function updatePromise(promise, parentPromise, status, value) {
  var i;
  for (i = 0; i < result.length; ++i) {
    if (result[i].promise === promise) {
      result[i].parentPromise = parentPromise || result[i].parentPromise;
      result[i].status = status || result[i].status;
      result[i].value = value || result[i].value;
      break;
    }
  }
  assertTrue(i < result.length);
}

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.PromiseEvent) return;
  try {
    eventsExpected--;
    assertTrue(event_data.promise().isPromise());
    if (event_data.status() === 0) {
      // New promise.
      assertEquals("pending", event_data.promise().status());
      result.push({ promise: event_data.promise().value(), status: 0 });
      assertTrue(exec_state.frame(0).sourceLineText().indexOf("// event") > 0);
    } else if (event_data.status() !== undefined) {
      // Resolve/reject promise.
      updatePromise(event_data.promise().value(),
                    undefined,
                    event_data.status(),
                    event_data.value().value());
    } else {
      // Chain promises.
      assertTrue(event_data.parentPromise().isPromise());
      updatePromise(event_data.promise().value(),
                    event_data.parentPromise().value());
      assertTrue(exec_state.frame(0).sourceLineText().indexOf("// event") > 0);
    }
  } catch (e) {
    print(e + e.stack)
    exception = e;
  }
}

Debug.setListener(listener);

function resolver(resolve, reject) { resolve(); }

var p1 = new Promise(resolver);  // event
var p2 = p1.then().then();  // event
var p3 = new Promise(function(resolve, reject) {  // event
  reject("rejected");
});
var p4 = p3.then();  // event
var p5 = p1.then();  // event

function assertAsync(b, s) {
  if (b) {
    print(s, "succeeded");
  } else {
    %AbortJS(s + " FAILED!");
  }
}

function testDone(iteration) {
  function checkResult() {
    if (eventsExpected === 0) {
      assertAsync(result.length === 6, "result.length");

      assertAsync(result[0].promise === p1, "result[0].promise");
      assertAsync(result[0].parentPromise === undefined,
                  "result[0].parentPromise");
      assertAsync(result[0].status === 1, "result[0].status");
      assertAsync(result[0].value === undefined, "result[0].value");

      assertAsync(result[1].parentPromise === p1,
                  "result[1].parentPromise");
      assertAsync(result[1].status === 1, "result[1].status");

      assertAsync(result[2].promise === p2, "result[2].promise");

      assertAsync(result[3].promise === p3, "result[3].promise");
      assertAsync(result[3].parentPromise === undefined,
                  "result[3].parentPromise");
      assertAsync(result[3].status === -1, "result[3].status");
      assertAsync(result[3].value === "rejected", "result[3].value");

      assertAsync(result[4].promise === p4, "result[4].promise");
      assertAsync(result[4].parentPromise === p3,
                  "result[4].parentPromise");
      assertAsync(result[4].status === -1, "result[4].status");
      assertAsync(result[4].value === "rejected", "result[4].value");

      assertAsync(result[5].promise === p5, "result[5].promise");
      assertAsync(result[5].parentPromise === p1,
                  "result[5].parentPromise");
      assertAsync(result[5].status === 1, "result[5].status");

      assertAsync(exception === null, "exception === null");
      Debug.setListener(null);
    } else if (iteration > 10) {
      %AbortJS("Not all events were received!");
    } else {
      testDone(iteration + 1);
    }
  }

  var iteration = iteration || 0;
  var dummy = {};
  Object.observe(dummy, checkResult);
  dummy.dummy = dummy;
}

testDone();
