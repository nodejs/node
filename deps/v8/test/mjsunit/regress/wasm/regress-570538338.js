// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

// d8's `send` dispatches protocol messages synchronously, so a Wasm breakpoint
// condition can disable the debugger while it is being evaluated. A second
// break point (from setBreakpointOnFunctionCall) at the same position must not
// be evaluated anymore afterwards.

let mod_bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x05,                    // section length 5
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $main (result i32)

  0x07,                    // section kind: Export
  0x08,                    // section length 8
  0x01,                    // exports count 1: export # 0
  0x04,                    // field name length:  4
  0x6d, 0x61, 0x69, 0x6e,  // field name: main
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x07,                    // section length 7
  0x01,                    // functions count 1
                           // function #0 $main
  0x05,                    // body size 5
  0x00,                    // 0 entries in locals list
  0x01,                    // nop
  0x41, 0x2a,              // i32.const 42
  0x0b,                    // end
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
