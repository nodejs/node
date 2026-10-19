// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

let evaluateResult;
globalThis.receive = function(message) {
  const msg = JSON.parse(message);
  if (msg.method === 'Debugger.paused') {
    const callFrameId = msg.params.callFrames[0].callFrameId;
    send(JSON.stringify({
      id: 2,
      method: 'Debugger.evaluateOnCallFrame',
      params: {
        callFrameId,
        expression:
            'let f = () => import.meta; let g = () => () => import.meta; ' +
            'f() === import.meta && g()() === import.meta',
        returnByValue: true,
      }
    }));
  } else if (msg.id === 2) {
    evaluateResult = msg.result;
  }
};

send(JSON.stringify({id: 1, method: 'Debugger.enable'}));
debugger;

assertFalse('exceptionDetails' in evaluateResult);
assertEquals('boolean', evaluateResult.result.type);
assertTrue(evaluateResult.result.value);

assertThrows(() => eval('let h = () => import.meta; h()'), SyntaxError);
