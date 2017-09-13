// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    var match = /Exception/.exec(line);
    assertNotNull(match);
  } catch (e) {
    exception = e;
  }
}

// Caught throw, events on any exception.
Debug.setListener(listener);
Debug.setBreakOnException();

var thenable = {
  get then() {
    throw new Error();  // Exception
  }
};

class MyPromise extends Promise {
    get then()  {
        throw new Error();
    }
}

MyPromise.resolve(thenable);

var p = Promise.resolve(thenable);

async function foo() {
  return thenable;
}

foo();

async function foo() {
  await thenable;
}

foo();

async function foo() {
  try {
    await thenable;
  } catch (e) {}
}

foo();

%RunMicrotasks();

Debug.setListener(null);
Debug.clearBreakOnException();
assertNull(exception);
