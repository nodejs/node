// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Test that destroying context during addBinding does not cause UAF (regress-517972812).');

(async function test() {
  const contextGroup = new InspectorTest.ContextGroup();
  const session = contextGroup.connect();

  session.Protocol.Runtime.enable();

  // Register binding 'x'
  await session.Protocol.Runtime.addBinding({name: 'x'});

  // Register second binding 'y' to test handling addBinding() again when the
  // the previous addBinding() has already destroyed the context.
  await session.Protocol.Runtime.addBinding({name: 'y'});

  InspectorTest.log('Triggering evaluate with prototype hijack and context creation...');
  await session.Protocol.Runtime.evaluate({
    expression: `
      delete globalThis.x;
      delete globalThis.y;
      Object.defineProperty(Object.prototype, 'x', {
        configurable: true,
        set: function(v) {
          // Synchronously destroy the just-registered InspectedContext.
          inspector.fireContextDestroyed();
        }
      });
    `,
  });

  // === TRIGGER ===
  session.reconnect();

  InspectorTest.log('Finished evaluate call.');
  InspectorTest.completeTest();
})();
