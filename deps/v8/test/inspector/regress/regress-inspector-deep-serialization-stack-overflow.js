// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Lower the C++ stack limit so 1000-level C++ recursion in
// `buildDeepSerializedValue` hits the stack overflow check on both native and
// simulator builds.
// Flags: --stack-size=100

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests that deep serialization gracefully reports stack overflow.');

contextGroup.addScript(`
// Build a 1000-level nested array matching kMaxProtocolDepth.
let deepArray = [];
for (let i = 0; i < 1000; i++) {
  deepArray = [deepArray];
}
`);

(async function test() {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: 'deepArray',
    serializationOptions: {serialization: 'deep'},
  }));
  InspectorTest.completeTest();
})();
