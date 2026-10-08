// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, Protocol} = InspectorTest.start(
    'Test that Runtime.evaluate with includeCommandLineAPI does not crash when global object is frozen.');

InspectorTest.runAsyncTestSuite([
  async function testStrictFrame() {
    await Protocol.Debugger.enable();

    // Define both strict and sloppy mode functions upfront before freezing the global object.
    await Protocol.Runtime.evaluate({
      expression: `
        const global = this;
        function runStrict() {
          "use strict";
          Object.freeze(global);
          debugger;
        }
        function runSloppy() {
          debugger;
        }
      `
    });

    // Run the strict function. It should pause.
    const pausedPromise = Protocol.Debugger.oncePaused();
    Protocol.Runtime.evaluate({ expression: 'runStrict()' });
    await pausedPromise;
    InspectorTest.log('Paused in strict mode.');

    // Evaluate with includeCommandLineAPI: true.
    // In strict mode, the command line API installation throws TypeError.
    // The evaluation aborts early and returns the TypeError object.
    InspectorTest.log('Evaluating with includeCommandLineAPI while paused in strict...');
    const evalResponse = await Protocol.Runtime.evaluate({
      expression: '2 + 2',
      includeCommandLineAPI: true
    });

    // Log the full response to see the error.
    InspectorTest.logMessage(evalResponse);

    await Protocol.Debugger.resume();
  },

  async function testSloppyFrame() {
    // Note: Global is already frozen from the previous test.
    // runSloppy is also already defined.

    // Run the sloppy function. It should pause.
    const pausedPromise = Protocol.Debugger.oncePaused();
    Protocol.Runtime.evaluate({ expression: 'runSloppy()' });
    await pausedPromise;
    InspectorTest.log('Paused in sloppy mode.');

    // Evaluate with includeCommandLineAPI: true.
    // In sloppy mode, the command line API installation fails silently.
    // The evaluation proceeds and returns 4.
    InspectorTest.log('Evaluating with includeCommandLineAPI while paused in sloppy...');
    const evalResponse = await Protocol.Runtime.evaluate({
      expression: '2 + 2',
      includeCommandLineAPI: true
    });

    // Log the full response, should be value: 4.
    InspectorTest.logMessage(evalResponse);

    await Protocol.Debugger.resume();
  }
]);
