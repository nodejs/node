// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/576534261.');

const source = `
  const s = 'x'.repeat(%StringMaxLength());
`;
contextGroup.addScript(source);

(async () => {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: '[/a/, s]',
    generatePreview: false,
    serializationOptions: {
      serialization: 'deep',
    },
  }));
  InspectorTest.completeTest();
})();
