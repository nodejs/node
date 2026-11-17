// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start(
    'Tests that an unresolved asynchronous Runtime.evaluate does not cause a ' +
    'use-after-free during inspector and session teardown.');

Protocol.Runtime.evaluate({
  expression: 'new Promise(() => {})',
  awaitPromise: true,
});
InspectorTest.completeTest();
