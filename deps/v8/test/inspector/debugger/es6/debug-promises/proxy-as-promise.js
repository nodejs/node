// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that Promise subclass returning a Proxy does not crash in debugger.');

const source = `
var log = [];

class P extends Promise {
  constructor(...args) {
    super(...args);
    return new Proxy(this, {
      get(target, property, receiver) {
        if (property in target) {
          return Reflect.get(target, property, receiver);
        } else {
          return (...args) =>
              new Promise((resolve, reject) =>
                  target.then(v => resolve(v[property](...args)))
                      .catch(reject)
              );
        }
      }
    });
  }
}

P.resolve({doStuff(){log.push(1)}}).doStuff();

%PerformMicrotaskCheckpoint();
`;

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});

  let events = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames, data}}) => {
    events++;
    let message = data.description.split('\n')[0].replace(/^TypeError:\s*/, '');
    InspectorTest.log(`Paused on exception: uncaught=${data.uncaught}`);
    InspectorTest.log(`Exception message: ${message}`);
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  let {result: {result: logResult}} =
      await Protocol.Runtime.evaluate({expression: 'log', returnByValue: true});
  InspectorTest.log(`Total exception events: ${events}`);
  InspectorTest.log(`Log: ${JSON.stringify(logResult.value)}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
