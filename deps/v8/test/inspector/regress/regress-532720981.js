// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that sending a malformed protocol message with a throwing global ' +
    'prototype Proxy does not crash.');

const source = `
Object.setPrototypeOf(globalThis, new Proxy({}, {
  get(target, prop) {
    if (prop === 'receive') {
      throw new Error('Triggering error crash during send()!');
    }
    return Reflect.get(target, prop);
  }
}));
`;

(async function test() {
  contextGroup.addScript(source);
  session.sendRawCommand(1, 'test message', () => {});
  await Protocol.Runtime.evaluate({expression: '1'});
  InspectorTest.completeTest();
})();
