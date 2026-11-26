// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

// Test that a TerminationException thrown inside Error.prepareStackTrace while
// copying a cross-realm ShadowRealm error is propagated rather than swallowed
// into a catchable TypeError by ShadowRealmConstructTypeErrorCopy.

if (typeof Worker !== 'undefined') {
  // 1. Cross-realm error thrown from ShadowRealm.prototype.evaluate.
  const worker = new Worker(`
    const realm = new ShadowRealm();
    const installPrepareStackTrace = realm.evaluate(\`
      (terminate) => {
        Error.prepareStackTrace = () => {
          terminate();
        };
      }
    \`);
    installPrepareStackTrace(() => d8.terminateNow());
    try {
      realm.evaluate("throw new Error('boom');");
    } catch (e) {
      postMessage(e.message);
    }
  `, {type: 'string'});
  assertEquals(undefined, worker.getMessage());
}

// 2. Cross-realm error thrown from a ShadowRealm wrapped function call.
const realm = new ShadowRealm();
const makeThrower = realm.evaluate(`
  (terminate) => {
    Error.prepareStackTrace = () => {
      terminate();
    };
    return () => {
      throw new Error('boom');
    };
  }
`);
const throwInRealm = makeThrower(() => d8.terminateNow());

try {
  throwInRealm();
} catch (e) {
  assertUnreachable();
}
