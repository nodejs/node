// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Test session disconnect during custom preview wrapping.');

const contextGroup = new InspectorTest.ContextGroup();
const victim = contextGroup.connect();
const VP = victim.Protocol;

const controller = contextGroup.connect();
const CP = controller.Protocol;

let freed = false;

(async function test() {
  await VP.Runtime.enable();
  await VP.Debugger.enable();
  await VP.Runtime.setCustomObjectFormatterEnabled({enabled: true});

  await CP.Runtime.enable();
  await CP.Debugger.enable();

  const victimId = victim.id;

  CP.Debugger.onPaused(async () => {
    if (freed) { await CP.Debugger.resume(); return; }
    freed = true;
    InspectorTest.log('[controller] paused -> stock utils.disconnectSession(victim)');
    utils.disconnectSession(victimId);
    InspectorTest.log('[controller] victim freed; resuming nested loop');
    await CP.Debugger.resume();
  });

  await VP.Runtime.evaluate({
    expression: `
      globalThis.devtoolsFormatters = [{
        header: function(obj, config) {
          console.error("Inside formatter");
          debugger;
          return null;
        },
        hasBody: function() { return false; },
      }];
      setTimeout(function() {
        console.log({a: 1});
      }, 0);
    `,
  });

  await CP.Runtime.evaluate({
    expression: 'new Promise(r => setTimeout(r, 0))',
    awaitPromise: true,
  });

  InspectorTest.log('Reached post-wait.');
  InspectorTest._sessions.delete(victim);
  InspectorTest.completeTest();
})();
