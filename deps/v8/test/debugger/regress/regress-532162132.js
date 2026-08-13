// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

let next_id = 2;
function receive(message) {
  let msg = JSON.parse(message);
  if (msg.method === "Debugger.paused") {
    let callFrameId = msg.params.callFrames[0].callFrameId;
    send(JSON.stringify({
      id: next_id++,
      method: "Debugger.evaluateOnCallFrame",
      params: {
        callFrameId: callFrameId,
        expression: "var obj = local_func(); obj[0] = 1;"
      }
    }));
  } else if (msg.id === 2) {
    print("Eval result:", JSON.stringify(msg));
    send(JSON.stringify({id: next_id++, method: "Debugger.resume"}));
  }
}
send(JSON.stringify({id: 1, method: "Debugger.enable"}));
function test() {
  var x = 123;
  var local_func = function() { return this; };
  debugger;
}
test();
