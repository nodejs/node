// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-promise-finally

Debug = debug.Debug

var exception = null;
var step = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    var match = /Exception/.exec(line);
    assertNotNull(match);
    step++;
  } catch (e) {
    exception = e;
  }
}

// Caught throw, events on any exception.
Debug.setListener(listener);
Debug.setBreakOnException();

var thenable = {
  get then() {
    throw new Error('err');  // Exception
  }
};

var caughtException = null;

Promise.resolve()
  .finally(() => thenable)
  .catch(e => caughtException = e);

%RunMicrotasks();

Debug.setListener(null);
Debug.clearBreakOnException();
assertNull(exception);
assertNotNull(caughtException);
assertEquals(1, step);
