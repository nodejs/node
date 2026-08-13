// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-inspector --turbofan --no-maglev

let msgId = 1;
function cmd(method, params) {
  return JSON.stringify({id: msgId++, method: method, params: params || {}});
}

let paused_locations = [];
function receive(msg) {
  let obj = JSON.parse(msg);
  if (obj.method === "Debugger.paused") {
    let fnName = obj.params.callFrames[0].functionName;
    paused_locations.push(fnName);
    if (fnName === "foo") {
      send(cmd("Debugger.stepOut"));
    } else if (fnName === "osr_caller") {
      send(cmd("Debugger.stepInto"));
    } else {
      send(cmd("Debugger.resume"));
    }
  }
}

function bar() {
  // Should pause here when stepping out of foo() from osr_caller()
}
%NeverOptimizeFunction(bar);

function foo() {
  debugger;
}
%NeverOptimizeFunction(foo);

function osr_caller() {
  for (let i = 0; i < 20; i++) {
    if (i === 10) {
      %OptimizeOsr();
    }
  }
  foo();
  bar();
}

send(cmd("Debugger.enable"));
%PrepareFunctionForOptimization(osr_caller);
osr_caller();

assertEquals(["foo", "osr_caller", "bar"], paused_locations);
