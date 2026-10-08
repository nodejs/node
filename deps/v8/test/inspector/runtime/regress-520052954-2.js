// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Test session disconnect during console inspect.');

const contextGroup = new InspectorTest.ContextGroup();
const victim = contextGroup.connect();
const VP = victim.Protocol;

const controller = contextGroup.connect();
const CP = controller.Protocol;

let freed = false;

(async function test() {
  await VP.Runtime.enable();
  await VP.Debugger.enable();

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
    expression: 'globalThis.savedInspect = inspect;',
    includeCommandLineAPI: true,
  });

  await VP.Runtime.evaluate({
    expression: `
      let e = new Error();
      delete e.name;
      Object.defineProperty(Object.getPrototypeOf(e), 'name', {
        get() {
          debugger;
          return '';
        }
      });
      setTimeout(function() {
        savedInspect(e);
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
