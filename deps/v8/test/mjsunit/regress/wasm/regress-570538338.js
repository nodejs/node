// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

// d8's `send` dispatches protocol messages synchronously, so a Wasm breakpoint
// condition can disable the debugger while it is being evaluated. A second
// break point (from setBreakpointOnFunctionCall) at the same position must not
// be evaluated anymore afterwards.

let mod_bytes = new Uint8Array([
  0, 97, 115, 109, 1, 0, 0, 0,
  1, 5, 1, 96, 0, 1, 127,
  3, 2, 1, 0,
  7, 8, 1, 4, 109, 97, 105, 110, 0, 0,
  10, 7, 1, 5, 0, 1, 65, 42, 11,
]);

let msgQueue = [];
function queueSend(msgObj) {
  msgQueue.push(msgObj);
  if (msgQueue.length === 1) setTimeout(processQueue);
}
function processQueue() {
  if (msgQueue.length > 0) send(JSON.stringify(msgQueue[0]));
}
function onResponse() {
  msgQueue.shift();
  processQueue();
}

let i1;
let cond1Evaluated = false;
let cond2Evaluated = false;
let mainResult;

globalThis.receive = function(msg_str) {
  let msg = JSON.parse(msg_str);
  if (msg.id && msgQueue.length > 0 && msg.id === msgQueue[0].id) {
    onResponse();
  }
  if (msg.method === 'Debugger.scriptParsed' &&
      msg.params.url.startsWith('wasm://')) {
    queueSend({
      id: 10,
      method: 'Debugger.getPossibleBreakpoints',
      params: {start: {scriptId: msg.params.scriptId, lineNumber: 0,
                       columnNumber: 0}}
    });
  }
  if (msg.id === 10) {
    queueSend({
      id: 1,
      method: 'Debugger.setBreakpoint',
      params: {location: msg.result.locations[0],
               condition: 'globalThis.cond1()'}
    });
    queueSend({
      id: 20,
      method: 'Runtime.evaluate',
      params: {expression: 'globalThis.i1.exports.main'}
    });
  }
  if (msg.id === 20) {
    queueSend({
      id: 2,
      method: 'Debugger.setBreakpointOnFunctionCall',
      params: {objectId: msg.result.result.objectId,
               condition: 'globalThis.cond2()'}
    });
    setTimeout(() => {
      mainResult = i1.exports.main();
    });
  }
};

globalThis.cond1 = function() {
  cond1Evaluated = true;
  send(JSON.stringify({id: 100, method: 'Debugger.disable'}));
  return false;
};
globalThis.cond2 = function() {
  cond2Evaluated = true;
  return false;
};

queueSend({id: 1000, method: 'Debugger.enable'});
globalThis.i1 = i1 =
    new WebAssembly.Instance(new WebAssembly.Module(mod_bytes));

// Runs after all the tasks above.
function check() {
  if (mainResult === undefined) {
    setTimeout(check);
    return;
  }
  assertEquals(42, mainResult);
  assertTrue(cond1Evaluated !== cond2Evaluated);
}
setTimeout(check);
