// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testCallSiteCrossRealmCapabilityFilter() {
  Error.stackTraceLimit = 20;

  let capturedSites = null;
  Error.prepareStackTrace = (_error, callSites) => {
    capturedSites = callSites;
    return callSites;
  };

  const realm1 = Realm.create();
  const captureInRealm1 = Realm.eval(realm1, `
    (() => {
      globalThis.realm1Receiver = {};

      function realm1Function(target) {
        Error.captureStackTrace(target);
      }

      return function(target) {
        realm1Function.call(globalThis.realm1Receiver, target);
      };
    })()
  `);

  const target = {};
  captureInRealm1(target);
  void target.stack;

  assertTrue(Array.isArray(capturedSites));
  assertTrue(capturedSites.length > 0);

  // All frames from realm1 must have their function, receiver,
  // and function name filtered out when accessed by the main realm.
  for (const site of capturedSites) {
    assertEquals(undefined, site.getFunction());
    assertEquals(undefined, site.getThis());
    assertEquals(null, site.getFunctionName());
  }
})();

(function testCallSiteSameRealmAccessAllowed() {
  const realm1 = Realm.create();

  const realm1Check = Realm.eval(realm1, `
    (() => {
      globalThis.sloppyReceiver = {};
      let site = null;
      Error.prepareStackTrace = (e, sites) => {
        site = sites.find(s => s.getFunctionName() === 'sloppyMethod');
        return sites;
      };

      (function sloppyMethod() {
        const err = new Error();
        void err.stack;
      }).call(globalThis.sloppyReceiver);

      return {
        hasFunctionName: site.getFunctionName() === 'sloppyMethod',
        hasFunction: typeof site.getFunction() === 'function',
        hasThis: site.getThis() === globalThis.sloppyReceiver,
      };
    })()
  `);
  assertTrue(realm1Check.hasFunctionName);
  assertTrue(realm1Check.hasFunction);
  assertTrue(realm1Check.hasThis);
})();
