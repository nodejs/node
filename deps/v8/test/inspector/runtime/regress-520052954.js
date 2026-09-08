// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Regression test for b/520052954.


InspectorTest.log('Test session disconnect during exception wrapping.');

const contextGroup = new InspectorTest.ContextGroup();

// Victim session.
const victim = contextGroup.connect();
const VP = victim.Protocol;

// Controller session.
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
  InspectorTest.log('victim session id = ' + victimId);

  CP.Debugger.onPaused(async () => {
    if (freed) { await CP.Debugger.resume(); return; }
    freed = true;
    InspectorTest.log('[controller] paused -> stock utils.disconnectSession(victim)');
    // Disconnect victim session.
    utils.disconnectSession(victimId);
    InspectorTest.log('[controller] victim freed; resuming nested loop');
    await CP.Debugger.resume();
  });

  await VP.Runtime.evaluate({
    expression: `
      globalThis.devtoolsFormatters = [{
        header: function(obj, config) { debugger; return null; },
        hasBody: function() { return false; },
      }];
      setTimeout(function() {
        // Trigger formatter.
        throw {tag: 'exc'};
      }, 0);
    `,
  });

  // Drive the scheduled setTimeout to completion from the CONTROLLER. If the UAF
  // fires under ASan we never get past here.
  await CP.Runtime.evaluate({
    expression: 'new Promise(r => setTimeout(r, 0))',
    awaitPromise: true,
  });

  InspectorTest.log('Reached post-wait (unexpected under ASan if bug fires).');
  InspectorTest._sessions.delete(victim);
  InspectorTest.completeTest();
})();
