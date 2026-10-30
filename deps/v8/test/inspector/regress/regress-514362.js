// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-514362.');

contextGroup.addScript(`
function bar(x) { debugger; }
function foo() { bar(arguments[0]); }
function wrap() { return foo(1); }

%PrepareFunctionForOptimization(wrap);
wrap();
wrap();
%OptimizeFunctionOnNextCall(wrap);
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(message => {
    const frames =
        message.params.callFrames.map(f => f.functionName || '(anonymous)');
    InspectorTest.log(`Frames: ${frames.join(', ')}`);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'wrap()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
