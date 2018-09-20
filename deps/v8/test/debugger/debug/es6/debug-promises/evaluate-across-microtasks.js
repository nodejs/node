// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;
var listenerComplete = false;
var exception = null;
var count = 0;
var log = [];
var done = false;

function LogX(x) {
  var stored_count = count;
  return function() {
    log.push(`[${stored_count}] ${x}`);
  };
}

function DebuggerStatement() {
  log.push(`[${count}] debugger`);
  if (count++ < 3) {
    debugger;
  }
}

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var p = Promise.resolve();
    var q = p.then(LogX("then 1"));
    p.then(LogX("then 2"));
    q.then(LogX("then 3"));
    q.then(DebuggerStatement);
    var r = q.then(() => { throw 1; });
    r.catch(LogX("catch"));
    listenerComplete = true;
  } catch (e) {
    exception = e;
    print(e, e.stack);
    quit(1);
  };
};

// Add the debug event listener.
Debug.setListener(listener);

DebuggerStatement();
LogX("start")();

// Make sure that the debug event listener was invoked.
assertTrue(listenerComplete);

%RunMicrotasks();

var expectation =
  [ "[0] debugger", "[1] start", "[1] then 1",
    "[1] then 2", "[1] then 3", "[1] debugger",
    "[2] then 1", "[2] then 2", "[1] catch",
    "[2] then 3", "[2] debugger", "[3] then 1",
    "[3] then 2", "[2] catch", "[3] then 3",
    "[3] debugger", "[3] catch",
  ];

assertEquals(expectation, log);
