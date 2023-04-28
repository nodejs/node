// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks preview for Error object');

(async function test() {
  const { result: { result } } = await Protocol.Runtime.evaluate({
    expression: `[]['\\n    at']()`,
    generatePreview: true
  });
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})()
