// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
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

// The first two frames are inside realm1. Their function, receiver,
// and function name must be filtered out when accessed by the main realm.
for (let i = 0; i < 2; i++) {
  const site = capturedSites[i];
  assertEquals("", site.toString());
  assertEquals(undefined, site.getFunction());
  assertEquals(undefined, site.getThis());
  assertEquals(null, site.getFunctionName());
}
})();
