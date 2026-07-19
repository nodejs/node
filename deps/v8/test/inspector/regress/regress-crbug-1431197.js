// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start('Test properties of TypedArrays backed with a resizable buffer');

(async () => {
  const { result } = await Protocol.Runtime.evaluate({
    expression: "new Float64Array(new ArrayBuffer(24, { maxByteLength: 24 }))",
    generatePreview: true,
  });
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
