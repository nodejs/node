// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

createSuite('Debugger.paused', 10000, DebuggerPaused, Setup, TearDown);
createSuite(
    'Debugger.getPossibleBreakpoints', 10000, DebuggerGetPossibleBreakpoints,
    SetupGetPossibleBreakpoints, TearDown);
createSuite(
    'AsyncStacksInstrumentation', 10000, AsyncStacksInstrumentation,
    SetupAsyncStacksInstrumentation, TearDown);

function Setup() {
  SendMessage('Debugger.enable');
  // Force lazy compilation of inspector related scripts.
  SendMessage('Runtime.evaluate', {expression: ''});
}

function TearDown() {
  SendMessage('Debugger.disable');
}

function DebuggerPaused() {
  for (var i = 0; i < 10; ++i) {
    debugger;
  }
}

let scriptId;
function SetupGetPossibleBreakpoints() {
  Setup();
  let expression = '';
  for (let i = 0; i < 20; ++i) {
    expression += `function foo${i}(){
        if (a) {
          return true;
        } else {
          return false;
        }
      }\n`;
  };
  listener = function(msg) {
    if (msg.method === 'Debugger.scriptParsed') {
      scriptId = msg.params.scriptId;
      listener = null;
    }
  };
  SendMessage('Runtime.evaluate', {expression});
}

function DebuggerGetPossibleBreakpoints() {
  SendMessage(
      'Debugger.getPossibleBreakpoints',
      {start: {lineNumber: 0, columnNumber: 0, scriptId: scriptId}});
}

function SetupAsyncStacksInstrumentation() {
  Setup();
  SendMessage('Debugger.setAsyncCallStackDepth', {maxDepth: 1024});
}

function AsyncStacksInstrumentation() {
  var p = Promise.resolve();
  var nopCallback = () => undefined;
  var done = false;
  for (let i = 0; i < 1000; ++i) {
    p = p.then(nopCallback);
  }
  p = p.then(() => done = true);
  while (!done) %RunMicrotasks();
}

})();
